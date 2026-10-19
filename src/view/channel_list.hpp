#pragma once

#include "model/channel.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtkmm.h>

namespace tv {

class ChannelListPane : public Gtk::Box {
 public:
  using PlayHandler = std::function<void(const std::string&)>;
  using StopHandler = std::function<void()>;
  using ActionHandler = std::function<void()>;

  ChannelListPane(PlayHandler on_play, StopHandler on_stop, ActionHandler on_edit,
                  ActionHandler on_remove, ActionHandler on_toggle_favorite);

  void reload(const std::vector<Channel>& channels,
              const std::optional<std::string>& select_id = std::nullopt,
              const std::optional<std::string>& playing_id = std::nullopt);
  void set_playing(const std::optional<std::string>& channel_id);
  void select(const std::string& channel_id);
  std::optional<std::string> selected_id() const;

 private:
  struct RowMeta {
    std::string channel_id;
    bool favorite = false;
    Gtk::Button* play_btn = nullptr;
    Gtk::Image* play_image = nullptr;
  };

  Gtk::ListBoxRow* make_row(const Channel& channel);
  void popup_channel_menu();
  void update_play_buttons();
  void on_play_clicked(const std::string& channel_id);
  bool on_button_press(GdkEventButton* event);

  PlayHandler on_play_;
  StopHandler on_stop_;
  ActionHandler on_edit_;
  ActionHandler on_remove_;
  ActionHandler on_toggle_favorite_;
  std::optional<std::string> playing_id_;
  std::map<Gtk::ListBoxRow*, RowMeta> rows_;
  Gtk::ListBox list_box_;
};

}  // namespace tv
