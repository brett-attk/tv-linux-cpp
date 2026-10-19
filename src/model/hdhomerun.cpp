#include "model/hdhomerun.hpp"

#include "infra/logging.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <curl/curl.h>

namespace tv {
namespace {

std::string trim_copy(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

std::string to_lower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

struct UrlParts {
  std::string scheme;
  std::string netloc;
  std::string hostname;
  std::string path;
};

bool parse_url(const std::string& url, UrlParts& out) {
  // Minimal parser: scheme://netloc/path
  const auto scheme_end = url.find("://");
  if (scheme_end == std::string::npos) {
    return false;
  }
  out.scheme = to_lower(url.substr(0, scheme_end));
  const size_t auth_start = scheme_end + 3;
  if (auth_start >= url.size()) {
    return false;
  }
  size_t path_start = url.find('/', auth_start);
  if (path_start == std::string::npos) {
    out.netloc = url.substr(auth_start);
    out.path.clear();
  } else {
    out.netloc = url.substr(auth_start, path_start - auth_start);
    out.path = url.substr(path_start);
  }
  if (out.netloc.empty()) {
    return false;
  }
  // hostname without userinfo/port
  std::string host = out.netloc;
  const auto at = host.rfind('@');
  if (at != std::string::npos) {
    host = host.substr(at + 1);
  }
  if (!host.empty() && host.front() == '[') {
    const auto bracket = host.find(']');
    out.hostname = bracket == std::string::npos ? host : host.substr(1, bracket - 1);
  } else {
    const auto colon = host.rfind(':');
    out.hostname = colon == std::string::npos ? host : host.substr(0, colon);
  }
  return !out.hostname.empty();
}

std::string join_url(const UrlParts& parts) {
  return parts.scheme + "://" + parts.netloc + parts.path;
}

std::string rstrip_slash(std::string s) {
  while (!s.empty() && s.back() == '/') {
    s.pop_back();
  }
  return s;
}

bool ends_with(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

size_t curl_write(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  const size_t total = size * nmemb;
  if (out->size() + total > kHdhrMaxResponseBytes + 1) {
    out->append(ptr, std::min(total, kHdhrMaxResponseBytes + 1 - out->size()));
    return 0;  // abort
  }
  out->append(ptr, total);
  return total;
}

[[noreturn]] void raise_connectivity(const std::string& message,
                                     const std::string& url) {
  log_error(message + " (" + url + ")");
  throw HdhrError(message, true, url);
}

std::vector<std::string> parse_tags(const nlohmann::json& raw) {
  std::vector<std::string> tags;
  if (raw.is_null()) {
    return tags;
  }
  if (raw.is_array()) {
    for (const auto& item : raw) {
      const std::string text = trim_copy(item.is_string() ? item.get<std::string>()
                                                          : item.dump());
      if (!text.empty()) {
        tags.push_back(text);
      }
    }
    return tags;
  }
  std::string text = trim_copy(raw.is_string() ? raw.get<std::string>() : raw.dump());
  if (text.empty()) {
    return tags;
  }
  std::stringstream ss(text);
  std::string part;
  while (std::getline(ss, part, ',')) {
    part = trim_copy(part);
    if (!part.empty()) {
      tags.push_back(part);
    }
  }
  return tags;
}

JsonGetter default_getter() {
  return [](const std::string& url, double timeout) {
    return http_get_json(url, timeout);
  };
}

}  // namespace

HdhrError::HdhrError(std::string message, bool connectivity,
                     std::optional<std::string> url)
    : std::runtime_error(std::move(message)),
      connectivity_(connectivity),
      url_(std::move(url)) {}

bool HdhrChannel::has_drm() const {
  for (const auto& tag : tags) {
    if (to_lower(tag) == "drm") {
      return true;
    }
  }
  return false;
}

std::string HdhrChannel::display_name() const {
  const std::string number = trim_copy(guide_number);
  const std::string name = trim_copy(guide_name);
  std::string label;
  if (!number.empty() && !name.empty()) {
    label = number + "  " + name;
  } else if (!number.empty()) {
    label = number;
  } else if (!name.empty()) {
    label = name;
  } else {
    label = url;
  }
  if (has_drm()) {
    label += "  (DRM)";
  }
  return label;
}

std::string normalize_device_url(const std::string& user_input) {
  std::string text = trim_copy(user_input);
  if (text.empty()) {
    throw HdhrError("Enter an HDHomeRun device URL or IP address.");
  }
  if (text.find("://") == std::string::npos) {
    text = "http://" + text;
  }
  UrlParts parts;
  if (!parse_url(text, parts) ||
      (parts.scheme != "http" && parts.scheme != "https")) {
    throw HdhrError("Enter a valid http:// URL or IP address.");
  }
  std::string path = parts.path;
  while (!path.empty() && path.back() == '/') {
    path.pop_back();
  }
  if (ends_with(path, "/lineup.json") || ends_with(path, "/discover.json")) {
    const auto slash = path.rfind('/');
    path = slash == std::string::npos ? std::string() : path.substr(0, slash);
  }
  parts.path = path;
  return rstrip_slash(join_url(parts));
}

std::string validate_fetch_url(const std::string& url,
                               const std::optional<std::string>& allowed_hostname) {
  const std::string text = trim_copy(url);
  UrlParts parts;
  if (!parse_url(text, parts) ||
      (parts.scheme != "http" && parts.scheme != "https") || parts.netloc.empty()) {
    throw HdhrError(
        "Refusing to fetch a non-http(s) URL from the HDHomeRun response.",
        false, text.empty() ? std::nullopt : std::optional<std::string>(text));
  }
  if (parts.hostname.empty()) {
    throw HdhrError("Refusing to fetch a URL without a hostname.", false, text);
  }
  if (allowed_hostname) {
    if (to_lower(parts.hostname) != to_lower(*allowed_hostname)) {
      throw HdhrError(
          "HDHomeRun returned a lineup URL on a different host; ignoring it.",
          false, text);
    }
  }
  return join_url(parts);
}

nlohmann::json http_get_json(const std::string& url, double timeout) {
  const std::string safe_url = validate_fetch_url(url);
  CURL* curl = curl_easy_init();
  if (!curl) {
    throw HdhrError("Could not initialize HTTP client.", true, url);
  }
  std::string body;
  long http_code = 0;
  curl_easy_setopt(curl, CURLOPT_URL, safe_url.c_str());
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "tv-cpp/1.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  const CURLcode rc = curl_easy_perform(curl);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (rc == CURLE_WRITE_ERROR && body.size() > kHdhrMaxResponseBytes) {
    log_error("Response from " + url + " exceeded max bytes");
    throw HdhrError("HDHomeRun response was too large to process.", false, url);
  }
  if (rc == CURLE_COULDNT_RESOLVE_HOST) {
    raise_connectivity(
        "Could not resolve the HDHomeRun hostname. Check the address and DNS.",
        url);
  }
  if (rc == CURLE_COULDNT_CONNECT) {
    raise_connectivity(
        "Connection refused by the HDHomeRun. Is the device powered on?", url);
  }
  if (rc == CURLE_OPERATION_TIMEDOUT) {
    raise_connectivity(
        "Timed out waiting for the HDHomeRun. Check network connectivity.",
        url);
  }
  if (rc == CURLE_SSL_CONNECT_ERROR || rc == CURLE_PEER_FAILED_VERIFICATION) {
    raise_connectivity("TLS/SSL error talking to the HDHomeRun.", url);
  }
  if (rc != CURLE_OK) {
    raise_connectivity(std::string("Could not reach the HDHomeRun device: ") +
                           curl_easy_strerror(rc),
                       url);
  }
  if (http_code == 404) {
    throw HdhrError(
        "HDHomeRun endpoint not found (404). Check the device address.", false,
        url);
  }
  if (http_code >= 500) {
    throw HdhrError(
        "HDHomeRun reported a server error (HTTP " + std::to_string(http_code) +
            "). The device may be busy or scanning channels.",
        false, url);
  }
  if (http_code >= 400) {
    throw HdhrError("HDHomeRun returned HTTP " + std::to_string(http_code) + ".",
                    false, url);
  }
  if (body.empty() ||
      std::all_of(body.begin(), body.end(),
                  [](unsigned char c) { return std::isspace(c); })) {
    throw HdhrError("HDHomeRun returned an empty response.", false, url);
  }
  try {
    return nlohmann::json::parse(body);
  } catch (const nlohmann::json::parse_error& exc) {
    log_error(std::string("Invalid JSON from ") + url + ": " + exc.what());
    throw HdhrError("HDHomeRun returned invalid JSON.", false, url);
  }
}

std::vector<HdhrChannel> parse_lineup(const nlohmann::json& payload) {
  nlohmann::json items;
  if (payload.is_object()) {
    const auto scan = payload.value("ScanInProgress", nlohmann::json(nullptr));
    if (scan == 1 || scan == true || scan == "1") {
      log_warning("HDHomeRun reports a channel scan is in progress");
      throw HdhrError(
          "HDHomeRun is scanning channels. Try again when the scan finishes.");
    }
    if (payload.contains("Channels")) {
      items = payload["Channels"];
    } else if (payload.contains("channels")) {
      items = payload["channels"];
    } else {
      items = nlohmann::json::array();
    }
  } else if (payload.is_array()) {
    items = payload;
  } else {
    throw HdhrError("Unexpected lineup.json format from the HDHomeRun.");
  }
  if (!items.is_array()) {
    throw HdhrError("Unexpected lineup.json format from the HDHomeRun.");
  }

  std::vector<HdhrChannel> channels;
  int skipped = 0;
  for (const auto& item : items) {
    if (!item.is_object()) {
      ++skipped;
      continue;
    }
    std::string url;
    if (item.contains("URL") && item["URL"].is_string()) {
      url = trim_copy(item["URL"].get<std::string>());
    } else if (item.contains("url") && item["url"].is_string()) {
      url = trim_copy(item["url"].get<std::string>());
    }
    if (url.empty()) {
      ++skipped;
      continue;
    }
    HdhrChannel channel;
    channel.url = url;
    if (item.contains("GuideNumber")) {
      channel.guide_number =
          trim_copy(item["GuideNumber"].is_string()
                        ? item["GuideNumber"].get<std::string>()
                        : item["GuideNumber"].dump());
    } else if (item.contains("guideNumber")) {
      channel.guide_number =
          trim_copy(item["guideNumber"].is_string()
                        ? item["guideNumber"].get<std::string>()
                        : item["guideNumber"].dump());
    }
    if (item.contains("GuideName")) {
      channel.guide_name =
          trim_copy(item["GuideName"].is_string()
                        ? item["GuideName"].get<std::string>()
                        : item["GuideName"].dump());
    } else if (item.contains("guideName")) {
      channel.guide_name =
          trim_copy(item["guideName"].is_string()
                        ? item["guideName"].get<std::string>()
                        : item["guideName"].dump());
    }
    if (item.contains("Tags")) {
      channel.tags = parse_tags(item["Tags"]);
    } else if (item.contains("tags")) {
      channel.tags = parse_tags(item["tags"]);
    }
    channels.push_back(std::move(channel));
  }
  if (skipped) {
    log_info("Skipped " + std::to_string(skipped) +
             " lineup entries without usable URLs");
  }
  return channels;
}

std::string resolve_lineup_url(const std::string& base_url, double timeout,
                               const JsonGetter& getter_arg) {
  const JsonGetter getter = getter_arg ? getter_arg : default_getter();
  const std::string base = normalize_device_url(base_url);
  UrlParts base_parts;
  parse_url(base, base_parts);
  const std::string discover_url = base + "/discover.json";
  nlohmann::json data;
  try {
    data = getter(discover_url, timeout);
  } catch (const HdhrError& exc) {
    if (exc.connectivity()) {
      throw;
    }
    log_warning("discover.json unavailable for " + base + "; falling back to lineup.json");
    return base + "/lineup.json";
  }

  if (data.is_object()) {
    std::string lineup;
    if (data.contains("LineupURL") && data["LineupURL"].is_string()) {
      lineup = trim_copy(data["LineupURL"].get<std::string>());
    } else if (data.contains("lineupURL") && data["lineupURL"].is_string()) {
      lineup = trim_copy(data["lineupURL"].get<std::string>());
    }
    if (!lineup.empty()) {
      try {
        const std::string safe =
            validate_fetch_url(lineup, base_parts.hostname);
        log_info("Resolved lineup URL via discover.json: " + safe);
        return safe;
      } catch (const HdhrError& exc) {
        log_warning("Ignoring untrusted LineupURL; using local fallback");
        (void)exc;
        return base + "/lineup.json";
      }
    }
    log_warning("discover.json missing LineupURL for " + base);
  } else {
    log_warning("discover.json was not an object for " + base);
  }
  return base + "/lineup.json";
}

std::vector<HdhrChannel> fetch_lineup(const std::string& user_url, double timeout,
                                      const JsonGetter& getter_arg) {
  const JsonGetter getter = getter_arg ? getter_arg : default_getter();
  log_info("Fetching HDHomeRun lineup for " + user_url);
  try {
    std::string text = trim_copy(user_url);
    if (text.find("://") == std::string::npos) {
      text = "http://" + text;
    }
    UrlParts parts;
    parse_url(text, parts);
    std::string path = parts.path;
    while (!path.empty() && path.back() == '/') {
      path.pop_back();
    }
    std::string lineup_url;
    if (ends_with(path, "/lineup.json")) {
      if ((parts.scheme != "http" && parts.scheme != "https") ||
          parts.netloc.empty()) {
        throw HdhrError("Enter a valid http:// URL or IP address.");
      }
      parts.path = path;
      lineup_url = validate_fetch_url(join_url(parts));
    } else {
      lineup_url = resolve_lineup_url(user_url, timeout, getter);
    }
    const auto payload = getter(lineup_url, timeout);
    auto channels = parse_lineup(payload);
    if (channels.empty()) {
      throw HdhrError(
          "No channels found in the HDHomeRun lineup. "
          "Confirm a channel scan has been completed on the device.",
          false, lineup_url);
    }
    log_info("Fetched " + std::to_string(channels.size()) + " channel(s) from " +
             lineup_url);
    return channels;
  } catch (const HdhrError&) {
    throw;
  } catch (const std::exception& exc) {
    log_exception(std::string("Unexpected failure fetching lineup: ") +
                  exc.what());
    throw HdhrError(std::string("Unexpected error talking to the HDHomeRun: ") +
                    exc.what());
  }
}

}  // namespace tv
