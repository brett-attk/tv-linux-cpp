#include "view/playback_bar.hpp"

#include <cstdio>

namespace tv {

PlaybackBar::PlaybackBar(Action on_prev, Action on_next, Action on_volume_down,
                         Action on_volume_up, ToggleAction on_mute,
                         ToggleAction on_osc, Action on_fullscreen,
                         bool osc_enabled)
    : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6),
      on_mute_(std::move(on_mute)),
      mute_btn_("Mute"),
      osc_btn_("OSC"),
      now_playing_("Now Playing: —", Gtk::ALIGN_START),
      volume_label_("", Gtk::ALIGN_END) {
  set_margin_top(6);
  set_margin_bottom(8);
  set_margin_start(8);
  set_margin_end(8);

  auto* prev_btn = Gtk::manage(new Gtk::Button("◀ Prev"));
  auto* next_btn = Gtk::manage(new Gtk::Button("Next ▶"));
  auto* vol_down_btn = Gtk::manage(new Gtk::Button("Vol −"));
  auto* vol_up_btn = Gtk::manage(new Gtk::Button("Vol +"));
  auto* fs_btn = Gtk::manage(new Gtk::Button("Fullscreen"));
  fs_btn->set_size_request(120, -1);

  osc_btn_.set_tooltip_text("Show on-screen controls on the video");
  osc_btn_.set_active(osc_enabled);

  prev_btn->signal_clicked().connect(std::move(on_prev));
  next_btn->signal_clicked().connect(std::move(on_next));
  vol_down_btn->signal_clicked().connect(std::move(on_volume_down));
  vol_up_btn->signal_clicked().connect(std::move(on_volume_up));
  mute_conn_ = mute_btn_.signal_toggled().connect(
      sigc::mem_fun(*this, &PlaybackBar::on_mute_toggled));
  osc_conn_ = osc_btn_.signal_toggled().connect([this, on_osc] {
    if (on_osc) {
      on_osc(osc_btn_.get_active());
    }
  });
  fs_btn->signal_clicked().connect(std::move(on_fullscreen));

  pack_start(*prev_btn, Gtk::PACK_SHRINK);
  pack_start(*next_btn, Gtk::PACK_SHRINK);
  pack_start(*vol_down_btn, Gtk::PACK_SHRINK);
  pack_start(*vol_up_btn, Gtk::PACK_SHRINK);
  pack_start(mute_btn_, Gtk::PACK_SHRINK);
  pack_start(osc_btn_, Gtk::PACK_SHRINK);
  pack_start(*fs_btn, Gtk::PACK_SHRINK);

  now_playing_.set_ellipsize(Pango::ELLIPSIZE_END);
  now_playing_.set_hexpand(true);
  now_playing_.set_width_chars(16);
  now_playing_.set_max_width_chars(16);
  pack_end(now_playing_, Gtk::PACK_EXPAND_WIDGET);

  volume_label_.set_width_chars(10);
  volume_label_.set_max_width_chars(10);
  volume_label_.set_text("Vol  80");
  pack_end(volume_label_, Gtk::PACK_SHRINK);
}

void PlaybackBar::set_now_playing(const std::optional<std::string>& name) {
  if (!name || name->empty()) {
    now_playing_.set_text("Now Playing: —");
  } else {
    now_playing_.set_text("Now Playing: " + *name);
  }
}

void PlaybackBar::set_volume(int volume, bool muted) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "Vol %3d%c", volume, muted ? 'M' : ' ');
  volume_label_.set_text(buf);
}

void PlaybackBar::set_muted(bool muted) {
  mute_conn_.block();
  mute_btn_.set_active(muted);
  mute_conn_.unblock();
}

void PlaybackBar::set_osc_active(bool enabled) {
  osc_conn_.block();
  osc_btn_.set_active(enabled);
  osc_conn_.unblock();
}

void PlaybackBar::on_mute_toggled() {
  if (on_mute_) {
    on_mute_(mute_btn_.get_active());
  }
}

}  // namespace tv
