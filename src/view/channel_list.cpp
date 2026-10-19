#include "view/channel_list.hpp"

namespace tv {
namespace {
constexpr const char* kPlayIcon = "media-playback-start-symbolic";
constexpr const char* kStopIcon = "media-playback-stop-symbolic";
constexpr const char* kFavoriteIcon = "emblem-favorite-symbolic";
constexpr int kBtnSize = 28;
constexpr int kHeartSize = 16;
}  // namespace

ChannelListPane::ChannelListPane(PlayHandler on_play, StopHandler on_stop,
                                 ActionHandler on_edit, ActionHandler on_remove,
                                 ActionHandler on_toggle_favorite)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6),
      on_play_(std::move(on_play)),
      on_stop_(std::move(on_stop)),
      on_edit_(std::move(on_edit)),
      on_remove_(std::move(on_remove)),
      on_toggle_favorite_(std::move(on_toggle_favorite)) {
  set_size_request(280, -1);
  set_hexpand(false);
  set_hexpand_set(true);
  set_margin_top(8);
  set_margin_bottom(8);
  set_margin_start(8);
  set_margin_end(4);

  auto* heading = Gtk::manage(new Gtk::Label("Channels", Gtk::ALIGN_START));
  heading->get_style_context()->add_class("title");
  pack_start(*heading, Gtk::PACK_SHRINK);

  auto* scrolled = Gtk::manage(new Gtk::ScrolledWindow());
  scrolled->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
  scrolled->set_propagate_natural_width(false);
  scrolled->set_vexpand(true);
  scrolled->set_hexpand(true);
  scrolled->set_shadow_type(Gtk::SHADOW_IN);

  list_box_.set_selection_mode(Gtk::SELECTION_SINGLE);
  list_box_.signal_row_selected().connect(
      [this](Gtk::ListBoxRow*) { update_play_buttons(); });
  list_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  list_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &ChannelListPane::on_button_press), false);
  scrolled->add(list_box_);
  pack_start(*scrolled, Gtk::PACK_EXPAND_WIDGET);
}

bool ChannelListPane::on_button_press(GdkEventButton* event) {
  if (!event || event->button != 3) {
    return false;
  }
  auto* row = list_box_.get_row_at_y(static_cast<int>(event->y));
  if (!row || rows_.find(row) == rows_.end()) {
    return false;
  }
  list_box_.select_row(*row);
  popup_channel_menu();
  return true;
}

void ChannelListPane::reload(const std::vector<Channel>& channels,
                             const std::optional<std::string>& select_id,
                             const std::optional<std::string>& playing_id) {
  playing_id_ = playing_id;
  for (auto* child : list_box_.get_children()) {
    list_box_.remove(*child);
  }
  rows_.clear();

  Gtk::ListBoxRow* selected = nullptr;
  for (const auto& channel : channels) {
    auto* row = make_row(channel);
    list_box_.append(*row);
    if (select_id && channel.id == *select_id) {
      selected = row;
    } else if (playing_id && channel.id == *playing_id && !selected) {
      selected = row;
    }
  }
  list_box_.show_all();
  if (selected) {
    list_box_.select_row(*selected);
  }
  update_play_buttons();
}

void ChannelListPane::set_playing(const std::optional<std::string>& channel_id) {
  playing_id_ = channel_id;
  update_play_buttons();
}

void ChannelListPane::select(const std::string& channel_id) {
  for (auto& [row, meta] : rows_) {
    if (meta.channel_id == channel_id) {
      list_box_.select_row(*row);
      return;
    }
  }
}

std::optional<std::string> ChannelListPane::selected_id() const {
  auto* row = list_box_.get_selected_row();
  if (!row) {
    return std::nullopt;
  }
  auto it = rows_.find(const_cast<Gtk::ListBoxRow*>(row));
  if (it == rows_.end()) {
    return std::nullopt;
  }
  return it->second.channel_id;
}

Gtk::ListBoxRow* ChannelListPane::make_row(const Channel& channel) {
  auto* row = Gtk::manage(new Gtk::ListBoxRow());
  auto* box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
  box->set_margin_start(8);
  box->set_margin_end(4);
  box->set_margin_top(4);
  box->set_margin_bottom(4);

  if (channel.favorite) {
    auto* heart = Gtk::manage(
        new Gtk::Image(Glib::ustring(kFavoriteIcon), Gtk::ICON_SIZE_MENU));
    heart->set_pixel_size(kHeartSize);
    heart->set_valign(Gtk::ALIGN_CENTER);
    box->pack_start(*heart, Gtk::PACK_SHRINK);
  }

  auto* label = Gtk::manage(new Gtk::Label(channel.name, Gtk::ALIGN_START));
  label->set_ellipsize(Pango::ELLIPSIZE_END);
  label->set_hexpand(true);
  box->pack_start(*label, Gtk::PACK_EXPAND_WIDGET);

  auto* slot = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  slot->set_size_request(kBtnSize, kBtnSize);
  slot->set_valign(Gtk::ALIGN_CENTER);

  auto* play_btn = Gtk::manage(new Gtk::Button());
  play_btn->set_relief(Gtk::RELIEF_NONE);
  play_btn->set_can_focus(false);
  play_btn->set_size_request(kBtnSize, kBtnSize);
  auto* image =
      Gtk::manage(new Gtk::Image(Glib::ustring(kPlayIcon), Gtk::ICON_SIZE_BUTTON));
  image->set_pixel_size(16);
  play_btn->set_image(*image);
  play_btn->set_always_show_image(true);
  play_btn->set_opacity(0.0);
  play_btn->set_sensitive(false);
  const std::string channel_id = channel.id;
  play_btn->signal_clicked().connect(
      [this, channel_id] { on_play_clicked(channel_id); });
  slot->pack_start(*play_btn, Gtk::PACK_EXPAND_WIDGET);
  box->pack_end(*slot, Gtk::PACK_SHRINK);

  row->add(*box);
  rows_[row] = RowMeta{channel.id, channel.favorite, play_btn, image};
  return row;
}

void ChannelListPane::popup_channel_menu() {
  // Replace any previous menu so we never leak Gtk::Menu instances.
  context_menu_ = std::make_unique<Gtk::Menu>();
  auto* row = list_box_.get_selected_row();
  bool favorite = false;
  if (row) {
    auto it = rows_.find(row);
    if (it != rows_.end()) {
      favorite = it->second.favorite;
    }
  }

  auto* fav_item = Gtk::manage(new Gtk::MenuItem(
      favorite ? "Remove from favorites" : "Favorite this channel"));
  fav_item->signal_activate().connect([this] {
    if (on_toggle_favorite_) {
      on_toggle_favorite_();
    }
  });
  context_menu_->append(*fav_item);

  auto* edit_item = Gtk::manage(new Gtk::MenuItem("Edit"));
  edit_item->signal_activate().connect([this] {
    if (on_edit_) {
      on_edit_();
    }
  });
  context_menu_->append(*edit_item);

  auto* remove_item = Gtk::manage(new Gtk::MenuItem("Remove"));
  remove_item->signal_activate().connect([this] {
    if (on_remove_) {
      on_remove_();
    }
  });
  context_menu_->append(*remove_item);

  context_menu_->show_all();
  context_menu_->popup_at_pointer(nullptr);
}

void ChannelListPane::on_play_clicked(const std::string& channel_id) {
  if (playing_id_ && *playing_id_ == channel_id) {
    if (on_stop_) {
      on_stop_();
    }
  } else if (on_play_) {
    on_play_(channel_id);
  }
}

void ChannelListPane::update_play_buttons() {
  auto* selected = list_box_.get_selected_row();
  for (auto& [row, meta] : rows_) {
    if (row == selected) {
      const bool is_playing = playing_id_ && *playing_id_ == meta.channel_id;
      meta.play_image->set_from_icon_name(is_playing ? kStopIcon : kPlayIcon,
                                          Gtk::ICON_SIZE_BUTTON);
      meta.play_image->set_pixel_size(16);
      meta.play_btn->set_tooltip_text(is_playing ? "Stop" : "Play");
      meta.play_btn->set_opacity(1.0);
      meta.play_btn->set_sensitive(true);
    } else {
      meta.play_btn->set_opacity(0.0);
      meta.play_btn->set_sensitive(false);
    }
  }
}

}  // namespace tv
