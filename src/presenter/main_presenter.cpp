#include "presenter/main_presenter.hpp"

#include "infra/logging.hpp"
#include "model/persist.hpp"
#include "view/dialogs.hpp"
#include "view/import_dialog.hpp"
#include "view/main_window.hpp"

#include <algorithm>
#include <stdexcept>

#include <gdk/gdkx.h>

namespace tv {

MainPresenter::MainPresenter(MainWindow& window)
    : window_(window),
      settings_(AppSettings::load()),
      store_(),
      player_(settings_.osc_enabled) {
  player_.set_fullscreen_handler([this](bool enabled) {
    Glib::signal_idle().connect_once([this, enabled] {
      if (enabled != ui_fullscreen_) {
        set_ui_fullscreen(enabled);
      }
    });
  });

  window_.controls().set_osc_active(settings_.osc_enabled);
  window_.channels().reload(store_.list());

  window_.video_area().signal_realize().connect(
      sigc::mem_fun(*this, &MainPresenter::on_video_realize));
  window_.signal_key_press_event().connect(
      sigc::mem_fun(*this, &MainPresenter::on_key_press), false);
  window_.signal_window_state_event().connect(
      sigc::mem_fun(*this, &MainPresenter::on_window_state));
}

MainPresenter::~MainPresenter() { player_.shutdown(); }

void MainPresenter::on_video_realize() {
  auto gdk_window = window_.video_area().get_window();
  if (!gdk_window) {
    log_error("Video area realized without a GDK window");
    return;
  }
#ifdef GDK_WINDOWING_X11
  if (!GDK_IS_X11_WINDOW(gdk_window->gobj())) {
    Glib::signal_idle().connect_once([this] {
      window_.show_error("Video embedding requires X11 (run via ./run.sh).");
    });
    return;
  }
  const auto wid =
      static_cast<int64_t>(gdk_x11_window_get_xid(gdk_window->gobj()));
#else
  const int64_t wid = 0;
#endif
  if (!wid) {
    Glib::signal_idle().connect_once([this] {
      window_.show_error("Could not get X11 window id for video pane.");
    });
    return;
  }
  try {
    player_.attach(wid);
  } catch (const std::exception& exc) {
    log_exception(std::string("Failed to attach mpv: ") + exc.what());
    Glib::signal_idle().connect_once([this, msg = std::string(exc.what())] {
      window_.show_error("Failed to embed mpv: " + msg + "\nDetails: " +
                         log_path().string());
    });
    return;
  }
  player_ready_ = true;
  log_info("mpv attached to wid=" + std::to_string(wid));
  const auto channels = store_.list();
  if (!channels.empty()) {
    window_.channels().select(channels.front().id);
  }
}

void MainPresenter::reload_list(const std::optional<std::string>& select_id) {
  std::optional<std::string> playing_id;
  if (current_) {
    playing_id = current_->id;
  }
  window_.channels().reload(store_.list(), select_id ? select_id : playing_id,
                            playing_id);
}

std::optional<Channel> MainPresenter::selected_channel() const {
  auto id = window_.channels().selected_id();
  if (!id) {
    return std::nullopt;
  }
  return store_.get(*id);
}

void MainPresenter::play_requested(const std::string& channel_id) {
  if (auto channel = store_.get(channel_id)) {
    play_channel(*channel);
  }
}

std::pair<int, int> MainPresenter::snapshot_window_size() const {
  int w = 0;
  int h = 0;
  window_.get_size(w, h);
  return {w, h};
}

bool MainPresenter::restore_window_size(std::pair<int, int> size) {
  int w = 0;
  int h = 0;
  window_.get_size(w, h);
  if (size.first > 1 && size.second > 1 &&
      (w != size.first || h != size.second)) {
    window_.resize(size.first, size.second);
  }
  return false;
}

void MainPresenter::schedule_size_restore(std::pair<int, int> size,
                                         bool delayed) {
  Glib::signal_idle().connect_once([this, size] { restore_window_size(size); });
  Glib::signal_timeout().connect_once(
      [this, size] { restore_window_size(size); }, 100);
  if (delayed) {
    Glib::signal_timeout().connect_once(
        [this, size] { restore_window_size(size); }, 500);
  }
}

void MainPresenter::play_channel(const Channel& channel) {
  const auto size = snapshot_window_size();
  current_ = channel;
  window_.controls().set_now_playing(channel.name);
  if (player_ready_) {
    player_.play(channel.url);
  }
  window_.channels().select(channel.id);
  window_.channels().set_playing(channel.id);
  schedule_size_restore(size, true);
}

void MainPresenter::stop_playback() {
  const auto size = snapshot_window_size();
  current_.reset();
  window_.controls().set_now_playing(std::nullopt);
  window_.channels().set_playing(std::nullopt);
  if (player_ready_) {
    player_.stop();
  }
  schedule_size_restore(size, false);
}

void MainPresenter::change_channel(int delta) {
  const auto channels = store_.list();
  if (channels.empty()) {
    return;
  }
  std::string current_id;
  if (current_) {
    current_id = current_->id;
  } else if (auto selected = selected_channel()) {
    current_id = selected->id;
  } else {
    current_id = channels.front().id;
  }
  if (auto neighbor = store_.neighbor(current_id, delta)) {
    play_channel(*neighbor);
  }
}

void MainPresenter::show_about_dialog() { show_about(window_); }

void MainPresenter::add_channel() {
  ChannelDialog dialog(window_, "Add Channel");
  while (true) {
    const int response = dialog.run();
    if (response != Gtk::RESPONSE_OK) {
      break;
    }
    auto values = dialog.get_values();
    if (!values) {
      continue;
    }
    auto [channel, err] = try_store_write<Channel>(
        [&] { return store_.add(values->first, values->second); });
    if (err) {
      log_error("Failed to save new channel: " + std::string(err->what()));
      window_.show_error("Could not save channel: " + std::string(err->what()) +
                         "\nDetails: " + log_path().string());
      break;
    }
    if (channel) {
      reload_list(channel->id);
      play_channel(*channel);
    }
    break;
  }
  dialog.hide();
}

void MainPresenter::import_channels() {
  std::vector<std::string> urls;
  for (const auto& c : store_.list()) {
    urls.push_back(c.url);
  }
  HdhrImportDialog dialog(window_, urls);
  const int response = dialog.run();
  auto selected = response == Gtk::RESPONSE_OK ? dialog.get_selected_channels()
                                               : std::vector<HdhrChannel>{};
  dialog.hide();
  if (selected.empty()) {
    return;
  }
  auto [result, err] = try_store_write<std::string>([&] {
    std::string id;
    for (const auto& channel : selected) {
      id = store_.add(channel.display_name(), channel.url).id;
    }
    return id;
  });
  if (err) {
    log_error("Failed while importing channels: " + std::string(err->what()));
    window_.show_error("Could not save imported channels: " +
                       std::string(err->what()) + "\nDetails: " +
                       log_path().string());
    reload_list(std::nullopt);
    return;
  }
  log_info("Imported " + std::to_string(selected.size()) +
           " HDHomeRun channel(s)");
  reload_list(result);
}

void MainPresenter::edit_channel() {
  auto channel = selected_channel();
  if (!channel) {
    return;
  }
  ChannelDialog dialog(window_, "Edit Channel", &*channel);
  while (true) {
    const int response = dialog.run();
    if (response != Gtk::RESPONSE_OK) {
      break;
    }
    auto values = dialog.get_values();
    if (!values) {
      continue;
    }
    const std::string old_url = channel->url;
    const std::string channel_id = channel->id;
    auto [updated, err] = try_store_write<Channel>([&] {
      auto result = store_.update(channel_id, values->first, values->second);
      if (!result) {
        throw std::runtime_error("channel missing");
      }
      return *result;
    });
    if (err) {
      log_error("Failed to save channel edits: " + std::string(err->what()));
      window_.show_error("Could not save channel: " + std::string(err->what()) +
                         "\nDetails: " + log_path().string());
      break;
    }
    if (updated) {
      reload_list(updated->id);
      if (current_ && current_->id == updated->id) {
        if (updated->url != old_url) {
          play_channel(*updated);
        } else {
          current_ = *updated;
          window_.controls().set_now_playing(updated->name);
          window_.channels().set_playing(updated->id);
        }
      }
    }
    break;
  }
  dialog.hide();
}

void MainPresenter::toggle_favorite() {
  auto channel = selected_channel();
  if (!channel) {
    return;
  }
  const std::string channel_id = channel->id;
  const bool favorite = !channel->favorite;
  auto [updated, err] = try_store_write<Channel>([&] {
    auto result = store_.set_favorite(channel_id, favorite);
    if (!result) {
      throw std::runtime_error("channel missing");
    }
    return *result;
  });
  if (err) {
    log_error("Failed to save favorite state: " + std::string(err->what()));
    window_.show_error("Could not save favorites: " + std::string(err->what()) +
                       "\nDetails: " + log_path().string());
    return;
  }
  if (updated) {
    reload_list(updated->id);
  }
}

void MainPresenter::remove_channel() {
  auto channel = selected_channel();
  if (!channel || !confirm_remove(window_, channel->name)) {
    return;
  }
  const bool was_playing = current_ && current_->id == channel->id;
  const int index = store_.index_of(channel->id);
  const std::string channel_id = channel->id;
  auto [removed, err] =
      try_store_write<bool>([&] { return store_.remove(channel_id); });
  (void)removed;
  if (err) {
    log_error("Failed to remove channel: " + std::string(err->what()));
    window_.show_error("Could not remove channel: " + std::string(err->what()) +
                       "\nDetails: " + log_path().string());
    return;
  }
  if (was_playing) {
    stop_playback();
  }
  const auto remaining = store_.list();
  std::optional<std::string> select_id;
  if (!remaining.empty() && index >= 0) {
    const int pick =
        std::min(index, static_cast<int>(remaining.size()) - 1);
    select_id = remaining[static_cast<size_t>(pick)].id;
  }
  reload_list(select_id);
}

void MainPresenter::volume_up() {
  window_.controls().set_volume(player_.volume_up(), player_.muted());
}

void MainPresenter::volume_down() {
  window_.controls().set_volume(player_.volume_down(), player_.muted());
}

void MainPresenter::mute_toggled(bool muted) {
  player_.set_mute(muted);
  window_.controls().set_volume(player_.volume(), player_.muted());
}

void MainPresenter::on_mute_key() {
  const bool muted = player_.mute_toggle();
  window_.controls().set_muted(muted);
  window_.controls().set_volume(player_.volume(), muted);
}

void MainPresenter::osc_toggled(bool enabled) {
  player_.set_osc_enabled(enabled);
  settings_.osc_enabled = enabled;
  try {
    settings_.save();
  } catch (const std::exception& exc) {
    log_exception(std::string("Failed to persist OSC setting: ") + exc.what());
    window_.show_error("Could not save settings: " + std::string(exc.what()) +
                       "\nDetails: " + log_path().string());
  }
}

void MainPresenter::fullscreen_clicked() {
  set_ui_fullscreen(!ui_fullscreen_);
}

void MainPresenter::set_ui_fullscreen(bool enabled) {
  ui_fullscreen_ = enabled;
  if (enabled) {
    window_.set_chrome_visible(false);
    window_.fullscreen();
  } else {
    window_.unfullscreen();
    window_.set_chrome_visible(true);
  }
  if (player_ready_) {
    player_.set_fullscreen(enabled);
  }
}

bool MainPresenter::on_window_state(GdkEventWindowState* event) {
  if (!event) {
    return false;
  }
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
    const bool is_fs =
        (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
    if (!is_fs && ui_fullscreen_) {
      ui_fullscreen_ = false;
      window_.set_chrome_visible(true);
      if (player_ready_) {
        player_.set_fullscreen(false);
      }
    }
  }
  return false;
}

bool MainPresenter::on_key_press(GdkEventKey* event) {
  if (!event) {
    return false;
  }
  const auto key = event->keyval;
  if (key == GDK_KEY_Escape && ui_fullscreen_) {
    set_ui_fullscreen(false);
    return true;
  }
  if ((key == GDK_KEY_f || key == GDK_KEY_F) &&
      !(event->state & GDK_CONTROL_MASK)) {
    if (dynamic_cast<Gtk::Entry*>(window_.get_focus())) {
      return false;
    }
    set_ui_fullscreen(!ui_fullscreen_);
    return true;
  }
  if (key == GDK_KEY_plus || key == GDK_KEY_equal || key == GDK_KEY_KP_Add) {
    volume_up();
    return true;
  }
  if (key == GDK_KEY_minus || key == GDK_KEY_KP_Subtract) {
    volume_down();
    return true;
  }
  if (key == GDK_KEY_m || key == GDK_KEY_M) {
    on_mute_key();
    return true;
  }
  if (key == GDK_KEY_Page_Up) {
    change_channel(-1);
    return true;
  }
  if (key == GDK_KEY_Page_Down) {
    change_channel(1);
    return true;
  }
  return false;
}

}  // namespace tv
