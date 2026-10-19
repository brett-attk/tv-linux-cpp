#pragma once

#include <filesystem>
#include <string>

namespace tv {

namespace fs = std::filesystem;

struct Paths {
  static fs::path config_dir();
  static fs::path state_dir();
  static fs::path channels_file();
  static fs::path settings_file();
  static fs::path error_log_file();
};

}  // namespace tv
