#include "view/dialogs.hpp"

#include "infra/app_icon.hpp"

#include <cctype>
#include <functional>
#include <regex>

namespace tv {
namespace {

bool valid_url(const std::string& url) {
  static const std::regex re(R"(^https?://[^/\s]+)", std::regex::icase);
  return std::regex_search(url, re);
}

void polish_about_dialog(Gtk::AboutDialog& dialog) {
  std::function<void(Gtk::Widget&)> walk = [&](Gtk::Widget& widget) {
    if (auto* label = dynamic_cast<Gtk::Label*>(&widget)) {
      label->set_selectable(false);
    }
    if (auto* container = dynamic_cast<Gtk::Container*>(&widget)) {
      for (auto* child : container->get_children()) {
        walk(*child);
      }
    }
  };
  walk(dialog);
}

}  // namespace

ChannelDialog::ChannelDialog(Gtk::Window& parent, const Glib::ustring& title,
                             const Channel* channel)
    : Gtk::Dialog(title, parent, true),
      error_label_("", Gtk::ALIGN_START) {
  add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  add_button("_OK", Gtk::RESPONSE_OK);
  set_default_response(Gtk::RESPONSE_OK);
  set_resizable(false);

  auto* grid = Gtk::manage(new Gtk::Grid());
  grid->set_column_spacing(8);
  grid->set_row_spacing(8);
  grid->set_margin_top(12);
  grid->set_margin_bottom(12);
  grid->set_margin_start(12);
  grid->set_margin_end(12);
  get_content_area()->add(*grid);

  auto* name_label = Gtk::manage(new Gtk::Label("Channel name:", Gtk::ALIGN_START));
  name_entry_.set_placeholder_text("NBC");
  name_entry_.set_hexpand(true);
  name_entry_.set_activates_default(true);

  auto* url_label = Gtk::manage(new Gtk::Label("URL:", Gtk::ALIGN_START));
  url_entry_.set_placeholder_text("http://192.168.1.27:5004/auto/v2.1");
  url_entry_.set_width_chars(42);
  url_entry_.set_hexpand(true);
  url_entry_.set_activates_default(true);

  if (channel) {
    name_entry_.set_text(channel->name);
    url_entry_.set_text(channel->url);
  }

  grid->attach(*name_label, 0, 0, 1, 1);
  grid->attach(name_entry_, 1, 0, 1, 1);
  grid->attach(*url_label, 0, 1, 1, 1);
  grid->attach(url_entry_, 1, 1, 1, 1);

  error_label_.get_style_context()->add_class("error");
  error_label_.set_no_show_all(true);
  grid->attach(error_label_, 0, 2, 2, 1);
  show_all();
}

std::optional<std::pair<std::string, std::string>> ChannelDialog::get_values() {
  const std::string name = name_entry_.get_text();
  const std::string url = url_entry_.get_text();
  auto trim = [](std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
      s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
      s.pop_back();
    }
    return s;
  };
  const std::string name_t = trim(name);
  const std::string url_t = trim(url);
  if (name_t.empty()) {
    error_label_.set_text("Enter a channel name.");
    error_label_.show();
    return std::nullopt;
  }
  if (!valid_url(url_t)) {
    error_label_.set_text("Enter an http:// or https:// URL.");
    error_label_.show();
    return std::nullopt;
  }
  return std::make_pair(name_t, url_t);
}

bool confirm_remove(Gtk::Window& parent, const std::string& channel_name) {
  Gtk::MessageDialog dialog(parent, "Remove channel \"" + channel_name + "\"?",
                            false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE,
                            true);
  dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dialog.add_button("Remove", Gtk::RESPONSE_OK);
  return dialog.run() == Gtk::RESPONSE_OK;
}

void show_about(Gtk::Window& parent) {
  Gtk::AboutDialog dialog;
  dialog.set_transient_for(parent);
  dialog.set_modal(true);
  dialog.set_program_name("TV");
  dialog.set_version(TV_CPP_VERSION);
  dialog.set_comments("GTK television front-end with embedded mpv playback.");
  dialog.set_website("https://github.com/brett-attk/linux-tv-cpp");
  dialog.set_website_label("GitHub repository");
  dialog.set_license_type(Gtk::LICENSE_MIT_X11);
  dialog.set_copyright("Licensed under the MIT License");
  if (auto icon = load_app_icon(128)) {
    dialog.set_logo(icon);
  } else {
    dialog.set_logo_icon_name("tv-cpp");
  }
  polish_about_dialog(dialog);
  dialog.run();
}

}  // namespace tv
