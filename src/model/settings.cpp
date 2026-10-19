#include "model/settings.hpp"

#include "infra/logging.hpp"
#include "infra/paths.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace tv {
namespace {
using json = nlohmann::json;
}  // namespace

AppSettings AppSettings::load(const fs::path& path_arg) {
  const fs::path path = path_arg.empty() ? Paths::settings_file() : path_arg;
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    AppSettings settings;
    try {
      settings.save(path);
    } catch (const std::exception& exc) {
      log_exception(std::string("Failed to create default settings: ") +
                    exc.what());
    }
    return settings;
  }
  try {
    std::ifstream in(path);
    if (!in) {
      return AppSettings{};
    }
    json data;
    in >> data;
    if (!data.is_object()) {
      log_error("Settings file is not a JSON object; using defaults");
      return AppSettings{};
    }
    AppSettings settings;
    settings.osc_enabled = data.value("osc_enabled", true);
    return settings;
  } catch (const std::exception& exc) {
    log_exception(std::string("Failed to load settings: ") + exc.what());
    return AppSettings{};
  }
}

void AppSettings::save(const fs::path& path_arg) const {
  const fs::path path = path_arg.empty() ? Paths::settings_file() : path_arg;
  try {
    fs::create_directories(path.parent_path());
    json payload{{"osc_enabled", osc_enabled}};
    const fs::path tmp = path.string() + ".tmp";
    {
      std::ofstream out(tmp);
      if (!out) {
        throw std::system_error(errno, std::generic_category(),
                                "Failed to write settings temp file");
      }
      out << payload.dump(2) << '\n';
    }
    fs::rename(tmp, path);
  } catch (const fs::filesystem_error& exc) {
    log_exception(std::string("Failed to save settings: ") + exc.what());
    throw std::system_error(exc.code(), exc.what());
  } catch (const std::system_error&) {
    log_exception("Failed to save settings to " + path.string());
    throw;
  }
}

}  // namespace tv
