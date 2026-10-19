#include "infra/app_icon.hpp"

#include "infra/logging.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <gtkmm.h>

namespace tv {
namespace {

namespace fs = std::filesystem;

std::vector<fs::path> icon_candidates(int size) {
  std::vector<fs::path> paths;

#ifdef TV_CPP_ICON_DIR
  const fs::path icon_dir = TV_CPP_ICON_DIR;
  paths.push_back(icon_dir / ("tv-cpp-" + std::to_string(size) + ".png"));
  paths.push_back(icon_dir / "tv-cpp.png");
#endif

  // Installed / XDG theme lookup via icon name is handled separately;
  // also try common relative locations for uninstalled runs.
  const char* home = std::getenv("HOME");
  if (home) {
    paths.push_back(fs::path(home) / ".local/share/icons/hicolor" /
                    (std::to_string(size) + "x" + std::to_string(size)) /
                    "apps/tv-cpp.png");
  }
  paths.push_back("/usr/local/share/icons/hicolor/" + std::to_string(size) +
                  "x" + std::to_string(size) + "/apps/tv-cpp.png");
  paths.push_back("/usr/share/icons/hicolor/" + std::to_string(size) + "x" +
                  std::to_string(size) + "/apps/tv-cpp.png");
  return paths;
}

}  // namespace

Glib::RefPtr<Gdk::Pixbuf> load_app_icon(int size) {
  // Prefer themed icon name when installed.
  try {
    if (Gtk::IconTheme::get_default()->has_icon("tv-cpp")) {
      return Gtk::IconTheme::get_default()->load_icon(
          "tv-cpp", size, Gtk::ICON_LOOKUP_USE_BUILTIN);
    }
  } catch (...) {
    // Fall through to file paths.
  }

  for (const auto& path : icon_candidates(size)) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
      continue;
    }
    try {
      return Gdk::Pixbuf::create_from_file(path.string(), size, size, true);
    } catch (const Glib::Error& exc) {
      log_warning(std::string("Failed to load icon ") + path.string() + ": " +
                  exc.what());
    }
  }

  // Last resort: bundled path via compile-time icon dir, any size.
#ifdef TV_CPP_ICON_DIR
  try {
    const fs::path fallback = fs::path(TV_CPP_ICON_DIR) / "tv-cpp.png";
    if (fs::exists(fallback)) {
      return Gdk::Pixbuf::create_from_file(fallback.string(), size, size, true);
    }
  } catch (const Glib::Error& exc) {
    log_warning(std::string("Failed to load bundled icon: ") + exc.what());
  }
#endif
  return {};
}

void apply_app_icon() {
  auto icon = load_app_icon(256);
  if (!icon) {
    log_warning("TV application icon not found");
    return;
  }
  std::vector<Glib::RefPtr<Gdk::Pixbuf>> list;
  for (int size : {16, 32, 48, 64, 128, 256}) {
    auto sized = load_app_icon(size);
    if (sized) {
      list.push_back(sized);
    }
  }
  if (!list.empty()) {
    Gtk::Window::set_default_icon_list(list);
  } else {
    Gtk::Window::set_default_icon(icon);
  }
}

}  // namespace tv
