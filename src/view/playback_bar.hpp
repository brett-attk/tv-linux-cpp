#pragma once

#include <functional>
#include <optional>
#include <string>

#include <gtkmm.h>

namespace tv {

class PlaybackBar : public Gtk::Box {
 public:
  using Action = std::function<void()>;
  using ToggleAction = std::function<void(bool)>;

  PlaybackBar(Action on_prev, Action on_next, Action on_volume_down,
              Action on_volume_up, ToggleAction on_mute, ToggleAction on_osc,
              Action on_fullscreen, bool osc_enabled);

  void set_now_playing(const std::optional<std::string>& name);
  void set_volume(int volume, bool muted);
  void set_muted(bool muted);
  void set_osc_active(bool enabled);

 private:
  void on_mute_toggled();

  ToggleAction on_mute_;
  Gtk::ToggleButton mute_btn_;
  Gtk::ToggleButton osc_btn_;
  Gtk::Label now_playing_;
  Gtk::Label volume_label_;
  sigc::connection mute_conn_;
  sigc::connection osc_conn_;
};

}  // namespace tv
