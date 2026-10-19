#pragma once

#include <filesystem>

namespace tv {

namespace fs = std::filesystem;

struct AppSettings {
  bool osc_enabled = true;

  static AppSettings load(const fs::path& path = {});
  void save(const fs::path& path = {}) const;
};

}  // namespace tv
