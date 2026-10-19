#include "infra/logging.hpp"

#include "infra/paths.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace tv {
namespace {

constexpr std::uintmax_t kMaxBytes = 1'000'000;
constexpr int kBackupCount = 3;

std::mutex g_mutex;
bool g_configured = false;
fs::path g_log_path;

std::string timestamp() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const std::time_t t = clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

void rotate_if_needed(const fs::path& path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return;
  }
  const auto size = fs::file_size(path, ec);
  if (ec || size < kMaxBytes) {
    return;
  }
  for (int i = kBackupCount - 1; i >= 1; --i) {
    const fs::path src = path.string() + "." + std::to_string(i);
    const fs::path dst = path.string() + "." + std::to_string(i + 1);
    if (fs::exists(src, ec)) {
      fs::rename(src, dst, ec);
    }
  }
  const fs::path first = path.string() + ".1";
  fs::rename(path, first, ec);
}

void write_line(const char* level, const std::string& message) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_configured) {
    return;
  }
  rotate_if_needed(g_log_path);
  std::ofstream out(g_log_path, std::ios::app);
  if (!out) {
    return;
  }
  out << timestamp() << ' ' << level << " [tv-cpp] " << message << '\n';
}

}  // namespace

fs::path setup_logging(const fs::path& log_path_arg) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_configured) {
    return g_log_path.empty() ? (log_path_arg.empty() ? Paths::error_log_file()
                                                      : log_path_arg)
                              : g_log_path;
  }
  g_log_path = log_path_arg.empty() ? Paths::error_log_file() : log_path_arg;
  std::error_code ec;
  fs::create_directories(g_log_path.parent_path(), ec);
  g_configured = true;
  {
    std::ofstream out(g_log_path, std::ios::app);
    if (out) {
      out << timestamp() << " INFO [tv-cpp] Logging initialized (" << g_log_path
          << ")\n";
    }
  }
  return g_log_path;
}

fs::path log_path() {
  return g_log_path.empty() ? Paths::error_log_file() : g_log_path;
}

void log_info(const std::string& message) { write_line("INFO", message); }
void log_warning(const std::string& message) { write_line("WARNING", message); }
void log_error(const std::string& message) { write_line("ERROR", message); }

void log_exception(const std::string& message) {
  write_line("ERROR", message);
}

}  // namespace tv
