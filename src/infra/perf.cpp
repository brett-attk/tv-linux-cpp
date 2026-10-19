#include "infra/perf.hpp"

#include "infra/logging.hpp"
#include "infra/mpv_player.hpp"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#include <glibmm.h>

namespace tv {
namespace perf {
namespace {

std::mutex g_mutex;
bool g_marked = false;
Clock::time_point g_start{};
std::atomic<bool> g_ready_emitted{false};
sigc::connection g_metrics_conn;
MpvPlayer* g_player = nullptr;

bool env_truthy(const char* value) {
  if (!value || value[0] == '\0') {
    return false;
  }
  std::string lower(value);
  for (char& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return lower != "0" && lower != "false" && lower != "no" && lower != "off";
}

bool on_metrics_tick() {
  MpvPlayer* player = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    player = g_player;
  }
  if (!player || !enabled()) {
    return false;
  }
  const double fps = player->estimated_fps();
  const int64_t drops = player->frame_drop_count();
  std::fprintf(stdout, "PERF_MPV fps=%.3f drops=%lld\n", fps,
               static_cast<long long>(drops));
  std::fflush(stdout);
  return true;
}

}  // namespace

bool enabled() {
  return env_truthy(std::getenv("TV_CPP_PERF"));
}

void mark_start() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_start = Clock::now();
  g_marked = true;
}

Clock::time_point start_time() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_start;
}

int64_t elapsed_ms() {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_marked) {
    return 0;
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                              g_start)
      .count();
}

std::string channel_url() {
  const char* value = std::getenv("TV_CPP_PERF_CHANNEL_URL");
  return value ? std::string(value) : std::string();
}

void on_player_ready(MpvPlayer& player) {
  if (!enabled()) {
    log_info("perf: TV_CPP_PERF not enabled; skipping PERF_READY");
    return;
  }
  if (g_ready_emitted.exchange(true)) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_player = &player;
  }

  // Ensure harness can see lines immediately when stdout is redirected.
  setvbuf(stdout, nullptr, _IOLBF, 0);

  const int64_t ms = elapsed_ms();
  std::fprintf(stdout, "PERF_READY t_ms=%lld\n", static_cast<long long>(ms));
  std::fflush(stdout);
  log_info("PERF_READY t_ms=" + std::to_string(ms));

  if (g_metrics_conn) {
    g_metrics_conn.disconnect();
  }
  g_metrics_conn = Glib::signal_timeout().connect_seconds(
      [] { return on_metrics_tick(); }, 1);
}

void shutdown() {
  if (g_metrics_conn) {
    g_metrics_conn.disconnect();
  }
  std::lock_guard<std::mutex> lock(g_mutex);
  g_player = nullptr;
}

}  // namespace perf
}  // namespace tv
