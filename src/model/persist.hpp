#pragma once

#include <functional>
#include <optional>
#include <system_error>
#include <utility>

namespace tv {

template <typename T>
std::pair<std::optional<T>, std::optional<std::system_error>> try_store_write(
    const std::function<T()>& action) {
  try {
    return {action(), std::nullopt};
  } catch (const std::system_error& exc) {
    return {std::nullopt, exc};
  }
}

}  // namespace tv
