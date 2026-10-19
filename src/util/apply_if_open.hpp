#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace tv {

// Used so background fetch completions are ignored after destroy.
template <typename Fn, typename... Args>
auto apply_if_open(bool closed, Fn&& callback, Args&&... args)
    -> std::optional<std::invoke_result_t<Fn, Args...>> {
  if (closed) {
    return std::nullopt;
  }
  return std::forward<Fn>(callback)(std::forward<Args>(args)...);
}

}  // namespace tv
