#pragma once

#include "infra/mpv_player.hpp"
#include "model/channel.hpp"
#include "model/channel_store.hpp"
#include "model/settings.hpp"

#include <optional>
#include <string>
#include <utility>

#include <gdkmm.h>

namespace tv {

class MainWindow;

class MainPresenter {
 public:
  explicit MainPresenter(MainWindow& window);
  ~MainPresenter();

  void play_requested(const std::string& channel_id);
  void stop_playback();
  void change_channel(int delta);
  void add_channel();
  void import_channels();
  void edit_channel();
  void toggle_favorite();
  void remove_channel();
  void show_about_dialog();
  void volume_up();
  void volume_down();
  void mute_toggled(bool muted);
  void osc_toggled(bool enabled);
  void fullscreen_clicked();

 private:
  void on_video_realize();
  void reload_list(const std::optional<std::string>& select_id = std::nullopt);
  std::optional<Channel> selected_channel() const;
  void play_channel(const Channel& channel);
  void on_mute_key();
  void set_ui_fullscreen(bool enabled);
  bool on_window_state(GdkEventWindowState* event);
  bool on_key_press(GdkEventKey* event);

  std::pair<int, int> snapshot_window_size() const;
  bool restore_window_size(std::pair<int, int> size);
  void schedule_size_restore(std::pair<int, int> size, bool delayed);

  MainWindow& window_;
  AppSettings settings_;
  ChannelStore store_;
  MpvPlayer player_;
  std::optional<Channel> current_;
  bool ui_fullscreen_ = false;
  bool player_ready_ = false;
};

}  // namespace tv
