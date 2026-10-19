#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace tv {

constexpr double kHdhrDefaultTimeout = 8.0;
constexpr size_t kHdhrMaxResponseBytes = 2'000'000;

struct HdhrChannel {
  std::string guide_number;
  std::string guide_name;
  std::string url;
  std::vector<std::string> tags;

  bool has_drm() const;
  std::string display_name() const;
};

class HdhrError : public std::runtime_error {
 public:
  explicit HdhrError(std::string message, bool connectivity = false,
                     std::optional<std::string> url = std::nullopt);

  bool connectivity() const { return connectivity_; }
  const std::optional<std::string>& url() const { return url_; }

 private:
  bool connectivity_;
  std::optional<std::string> url_;
};

using JsonGetter = std::function<nlohmann::json(const std::string& url,
                                                double timeout)>;

std::string normalize_device_url(const std::string& user_input);
std::string validate_fetch_url(
    const std::string& url,
    const std::optional<std::string>& allowed_hostname = std::nullopt);

nlohmann::json http_get_json(const std::string& url, double timeout);
std::vector<HdhrChannel> parse_lineup(const nlohmann::json& payload);

std::string resolve_lineup_url(const std::string& base_url,
                               double timeout = kHdhrDefaultTimeout,
                               const JsonGetter& getter = {});

std::vector<HdhrChannel> fetch_lineup(const std::string& user_url,
                                      double timeout = kHdhrDefaultTimeout,
                                      const JsonGetter& getter = {});

}  // namespace tv
