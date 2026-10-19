#pragma once

#include <string>

namespace tv {

struct Channel {
  std::string name;
  std::string url;
  std::string id;
  bool favorite = false;
};

}  // namespace tv
