#pragma once

#include "model/channel.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tv {

namespace fs = std::filesystem;

class ChannelStore {
 public:
  explicit ChannelStore(fs::path path = {});

  virtual ~ChannelStore() = default;

  void load();
  virtual void save();

  std::vector<Channel> list() const;
  std::optional<Channel> get(const std::string& channel_id) const;
  int index_of(const std::string& channel_id) const;

  Channel add(const std::string& name, const std::string& url);
  std::optional<Channel> update(const std::string& channel_id,
                                const std::string& name,
                                const std::string& url);
  std::optional<Channel> set_favorite(const std::string& channel_id,
                                      bool favorite);
  bool remove(const std::string& channel_id);
  std::optional<Channel> neighbor(const std::string& channel_id,
                                  int delta) const;

  const fs::path& path() const { return path_; }

 protected:
  fs::path path_;
  std::vector<Channel> channels_;

 private:
  Channel* find_mut(const std::string& channel_id);
  static std::string new_id();
  static std::string trim(std::string s);
};

}  // namespace tv
