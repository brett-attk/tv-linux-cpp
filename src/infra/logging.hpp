#pragma once

#include <filesystem>
#include <string>

namespace tv {

namespace fs = std::filesystem;

fs::path setup_logging(const fs::path& log_path = {});
fs::path log_path();

void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);
void log_exception(const std::string& message);

}  // namespace tv
