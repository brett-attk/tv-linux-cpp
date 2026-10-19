#include "model/channel_store.hpp"

#include "infra/logging.hpp"
#include "infra/paths.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace tv {
namespace {

using json = nlohmann::json;

std::string random_hex_id() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<uint64_t> dist;
  std::ostringstream oss;
  oss << std::hex << dist(rng) << dist(rng);
  auto s = oss.str();
  if (s.size() < 32) {
    s.append(32 - s.size(), '0');
  }
  return s.substr(0, 32);
}

}  // namespace

ChannelStore::ChannelStore(fs::path path)
    : path_(path.empty() ? Paths::channels_file() : std::move(path)) {
  load();
}

std::string ChannelStore::new_id() { return random_hex_id(); }

std::string ChannelStore::trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

void ChannelStore::load() {
  std::error_code ec;
  if (!fs::exists(path_, ec)) {
    channels_.clear();
    save();
    return;
  }
  try {
    std::ifstream in(path_);
    if (!in) {
      throw std::system_error(errno, std::generic_category(),
                              "Failed to open channels file");
    }
    json data;
    in >> data;
    json items;
    if (data.is_object() && data.contains("channels")) {
      items = data["channels"];
    } else if (data.is_array()) {
      items = data;
    } else {
      items = json::array();
    }

    std::vector<Channel> loaded;
    if (items.is_array()) {
      for (const auto& item : items) {
        if (!item.is_object()) {
          continue;
        }
        const std::string name = trim(item.value("name", ""));
        const std::string url = trim(item.value("url", ""));
        if (name.empty() || url.empty()) {
          continue;
        }
        Channel ch;
        ch.id = item.value("id", "");
        if (ch.id.empty()) {
          ch.id = new_id();
        }
        ch.name = name;
        ch.url = url;
        ch.favorite = item.value("favorite", false);
        loaded.push_back(std::move(ch));
      }
    }
    channels_ = std::move(loaded);
    if (channels_.empty()) {
      save();
    }
    log_info("Loaded " + std::to_string(channels_.size()) +
             " channel(s) from " + path_.string());
  } catch (const std::exception& exc) {
    log_exception(std::string("Failed to load channels: ") + exc.what());
    channels_.clear();
    try {
      save();
    } catch (const std::exception& save_exc) {
      log_exception(std::string("Failed to rewrite channels file: ") +
                    save_exc.what());
    }
  }
}

void ChannelStore::save() {
  try {
    fs::create_directories(path_.parent_path());
    json payload;
    payload["channels"] = json::array();
    for (const auto& c : channels_) {
      payload["channels"].push_back({
          {"id", c.id},
          {"name", c.name},
          {"url", c.url},
          {"favorite", c.favorite},
      });
    }
    const fs::path tmp = path_.string() + ".tmp";
    {
      std::ofstream out(tmp);
      if (!out) {
        throw std::system_error(errno, std::generic_category(),
                                "Failed to write channels temp file");
      }
      out << payload.dump(2) << '\n';
      if (!out) {
        throw std::system_error(errno, std::generic_category(),
                                "Failed to flush channels temp file");
      }
    }
    fs::rename(tmp, path_);
  } catch (const fs::filesystem_error& exc) {
    log_exception(std::string("Failed to save channels: ") + exc.what());
    throw std::system_error(exc.code(), exc.what());
  } catch (const std::system_error&) {
    log_exception("Failed to save channels to " + path_.string());
    throw;
  }
}

std::vector<Channel> ChannelStore::list() const {
  std::vector<Channel> favorites;
  std::vector<Channel> others;
  favorites.reserve(channels_.size());
  others.reserve(channels_.size());
  for (const auto& c : channels_) {
    if (c.favorite) {
      favorites.push_back(c);
    } else {
      others.push_back(c);
    }
  }
  favorites.insert(favorites.end(), others.begin(), others.end());
  return favorites;
}

Channel* ChannelStore::find_mut(const std::string& channel_id) {
  for (auto& c : channels_) {
    if (c.id == channel_id) {
      return &c;
    }
  }
  return nullptr;
}

std::optional<Channel> ChannelStore::get(const std::string& channel_id) const {
  for (const auto& c : channels_) {
    if (c.id == channel_id) {
      return c;
    }
  }
  return std::nullopt;
}

int ChannelStore::index_of(const std::string& channel_id) const {
  const auto ordered = list();
  for (size_t i = 0; i < ordered.size(); ++i) {
    if (ordered[i].id == channel_id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

Channel ChannelStore::add(const std::string& name, const std::string& url) {
  Channel channel;
  channel.name = trim(name);
  channel.url = trim(url);
  channel.id = new_id();
  channel.favorite = false;
  channels_.push_back(channel);
  try {
    save();
  } catch (...) {
    channels_.pop_back();
    throw;
  }
  return channel;
}

std::optional<Channel> ChannelStore::update(const std::string& channel_id,
                                            const std::string& name,
                                            const std::string& url) {
  Channel* channel = find_mut(channel_id);
  if (!channel) {
    return std::nullopt;
  }
  const auto previous_name = channel->name;
  const auto previous_url = channel->url;
  channel->name = trim(name);
  channel->url = trim(url);
  try {
    save();
  } catch (...) {
    channel->name = previous_name;
    channel->url = previous_url;
    throw;
  }
  return *channel;
}

std::optional<Channel> ChannelStore::set_favorite(const std::string& channel_id,
                                                  bool favorite) {
  Channel* channel = find_mut(channel_id);
  if (!channel) {
    return std::nullopt;
  }
  const bool previous = channel->favorite;
  channel->favorite = favorite;
  try {
    save();
  } catch (...) {
    channel->favorite = previous;
    throw;
  }
  return *channel;
}

bool ChannelStore::remove(const std::string& channel_id) {
  const auto previous = channels_;
  channels_.erase(std::remove_if(channels_.begin(), channels_.end(),
                                 [&](const Channel& c) {
                                   return c.id == channel_id;
                                 }),
                  channels_.end());
  if (channels_.size() == previous.size()) {
    return false;
  }
  try {
    save();
  } catch (...) {
    channels_ = previous;
    throw;
  }
  return true;
}

std::optional<Channel> ChannelStore::neighbor(const std::string& channel_id,
                                              int delta) const {
  const auto channels = list();
  if (channels.empty()) {
    return std::nullopt;
  }
  int idx = -1;
  for (size_t i = 0; i < channels.size(); ++i) {
    if (channels[i].id == channel_id) {
      idx = static_cast<int>(i);
      break;
    }
  }
  if (idx < 0) {
    return channels.front();
  }
  const int n = static_cast<int>(channels.size());
  const int next = ((idx + delta) % n + n) % n;
  return channels[static_cast<size_t>(next)];
}

}  // namespace tv
