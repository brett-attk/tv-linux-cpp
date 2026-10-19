#pragma once

#include "model/channel.hpp"

#include <optional>
#include <string>
#include <utility>

#include <gtkmm.h>

namespace tv {

class ChannelDialog : public Gtk::Dialog {
 public:
  ChannelDialog(Gtk::Window& parent, const Glib::ustring& title,
                const Channel* channel = nullptr);

  std::optional<std::pair<std::string, std::string>> get_values();

 private:
  Gtk::Entry name_entry_;
  Gtk::Entry url_entry_;
  Gtk::Label error_label_;
};

bool confirm_remove(Gtk::Window& parent, const std::string& channel_name);
void show_about(Gtk::Window& parent);

}  // namespace tv
