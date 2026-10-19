#include "infra/theme.hpp"

#include "infra/logging.hpp"

#include <optional>
#include <string>

#include <giomm.h>
#include <gtkmm.h>

namespace tv {
namespace {

constexpr const char* kSchemaId = "org.gnome.desktop.interface";
constexpr const char* kColorSchemeKey = "color-scheme";

Glib::RefPtr<Gio::Settings> g_watched_settings;

std::optional<std::string> gnome_color_scheme() {
  auto source = Gio::SettingsSchemaSource::get_default();
  if (!source) {
    return std::nullopt;
  }
  auto schema = source->lookup(kSchemaId, true);
  if (!schema || !schema->has_key(kColorSchemeKey)) {
    return std::nullopt;
  }
  auto settings = Gio::Settings::create(kSchemaId);
  return settings->get_string(kColorSchemeKey);
}

std::optional<bool> prefers_dark() {
  auto scheme = gnome_color_scheme();
  if (!scheme) {
    return std::nullopt;
  }
  if (*scheme == "prefer-dark") {
    return true;
  }
  if (*scheme == "prefer-light" || *scheme == "default") {
    return false;
  }
  return std::nullopt;
}

void apply_color_scheme() {
  auto gtk_settings = Gtk::Settings::get_default();
  if (!gtk_settings) {
    return;
  }
  auto prefers = prefers_dark();
  if (!prefers) {
    log_info("No desktop color-scheme preference available; using GTK defaults");
    return;
  }
  bool current = false;
  gtk_settings->get_property("gtk-application-prefer-dark-theme", current);
  if (current != *prefers) {
    gtk_settings->set_property("gtk-application-prefer-dark-theme", *prefers);
    log_info(std::string("gtk-application-prefer-dark-theme=") +
             (*prefers ? "true" : "false"));
  }
}

}  // namespace

void init_color_scheme() {
  apply_color_scheme();

  auto source = Gio::SettingsSchemaSource::get_default();
  if (!source) {
    return;
  }
  auto schema = source->lookup(kSchemaId, true);
  if (!schema || !schema->has_key(kColorSchemeKey)) {
    return;
  }

  g_watched_settings = Gio::Settings::create(kSchemaId);
  g_watched_settings->signal_changed().connect([](const Glib::ustring& key) {
    if (key == kColorSchemeKey) {
      apply_color_scheme();
    }
  });
}

}  // namespace tv
