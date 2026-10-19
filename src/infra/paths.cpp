#include "infra/paths.hpp"

#include <cstdlib>

namespace tv {
namespace {

fs::path xdg_home(const char* env_key, const fs::path& fallback_rel) {
  if (const char* value = std::getenv(env_key)) {
    if (value[0] != '\0') {
      return fs::path(value) / "tv-cpp";
    }
  }
  const char* home = std::getenv("HOME");
  fs::path base = home ? fs::path(home) : fs::path(".");
  return base / fallback_rel / "tv-cpp";
}

}  // namespace

fs::path Paths::config_dir() {
  return xdg_home("XDG_CONFIG_HOME", ".config");
}

fs::path Paths::state_dir() {
  return xdg_home("XDG_STATE_HOME", ".local/state");
}

fs::path Paths::channels_file() {
  return config_dir() / "channels.json";
}

fs::path Paths::settings_file() {
  return config_dir() / "settings.json";
}

fs::path Paths::error_log_file() {
  return state_dir() / "error.log";
}

}  // namespace tv
