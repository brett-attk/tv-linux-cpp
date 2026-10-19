#include "view/main_window.hpp"

#include "infra/app_icon.hpp"
#include "infra/logging.hpp"
#include "presenter/main_presenter.hpp"

namespace tv {

MainWindow::MainWindow() : Gtk::ApplicationWindow(), menu_bar_() {
  set_title("TV");
  set_default_size(1100, 650);
  set_border_width(0);
  if (auto icon = load_app_icon(256)) {
    set_icon(icon);
  }
  build_ui();
  presenter_ = std::make_unique<MainPresenter>(*this);
  show_all();
}

MainWindow::~MainWindow() = default;

void MainWindow::build_ui() {
  auto* outer = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
  add(*outer);

  build_menu_bar();
  outer->pack_start(menu_bar_, Gtk::PACK_SHRINK);

  auto* root = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
  outer->pack_start(*root, Gtk::PACK_EXPAND_WIDGET);

  channels_ = Gtk::manage(new ChannelListPane(
      [this](const std::string& id) {
        if (presenter_) {
          presenter_->play_requested(id);
        }
      },
      [this] {
        if (presenter_) {
          presenter_->stop_playback();
        }
      },
      [this] {
        if (presenter_) {
          presenter_->edit_channel();
        }
      },
      [this] {
        if (presenter_) {
          presenter_->remove_channel();
        }
      },
      [this] {
        if (presenter_) {
          presenter_->toggle_favorite();
        }
      }));
  root->pack_start(*channels_, Gtk::PACK_SHRINK);

  auto* right = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
  right->set_hexpand(true);
  right->set_vexpand(true);

  video_area_.set_visible_window(true);
  video_area_.set_hexpand(true);
  video_area_.set_vexpand(true);
  video_area_.set_halign(Gtk::ALIGN_FILL);
  video_area_.set_valign(Gtk::ALIGN_FILL);
  video_area_.set_size_request(640, 360);
  Gdk::RGBA bg;
  bg.set_rgba(0.05, 0.05, 0.05, 1.0);
  video_area_.override_background_color(bg);
  video_area_.set_double_buffered(false);
  right->pack_start(video_area_, Gtk::PACK_EXPAND_WIDGET);

  controls_ = Gtk::manage(new PlaybackBar(
      [this] {
        if (presenter_) {
          presenter_->change_channel(-1);
        }
      },
      [this] {
        if (presenter_) {
          presenter_->change_channel(1);
        }
      },
      [this] {
        if (presenter_) {
          presenter_->volume_down();
        }
      },
      [this] {
        if (presenter_) {
          presenter_->volume_up();
        }
      },
      [this](bool muted) {
        if (presenter_) {
          presenter_->mute_toggled(muted);
        }
      },
      [this](bool enabled) {
        if (presenter_) {
          presenter_->osc_toggled(enabled);
        }
      },
      [this] {
        if (presenter_) {
          presenter_->fullscreen_clicked();
        }
      },
      true));
  right->pack_start(*controls_, Gtk::PACK_SHRINK);
  root->pack_start(*right, Gtk::PACK_EXPAND_WIDGET);
}

Gtk::MenuBar* MainWindow::build_menu_bar() {
  auto* settings_menu = Gtk::manage(new Gtk::Menu());
  auto* settings_root = Gtk::manage(new Gtk::MenuItem("Settings"));
  settings_root->set_submenu(*settings_menu);

  auto* add_item = Gtk::manage(new Gtk::MenuItem("Add Channel…"));
  add_item->signal_activate().connect([this] {
    if (presenter_) {
      presenter_->add_channel();
    }
  });
  settings_menu->append(*add_item);

  auto* import_item = Gtk::manage(new Gtk::MenuItem("Import from HDHomeRun…"));
  import_item->signal_activate().connect([this] {
    if (presenter_) {
      presenter_->import_channels();
    }
  });
  settings_menu->append(*import_item);

  settings_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));

  auto* about_item = Gtk::manage(new Gtk::MenuItem("About"));
  about_item->signal_activate().connect([this] {
    if (presenter_) {
      presenter_->show_about_dialog();
    }
  });
  settings_menu->append(*about_item);

  menu_bar_.append(*settings_root);
  return &menu_bar_;
}

void MainWindow::set_chrome_visible(bool visible) {
  menu_bar_.set_visible(visible);
  if (channels_) {
    channels_->set_visible(visible);
  }
  if (controls_) {
    controls_->set_visible(visible);
  }
}

void MainWindow::show_error(const std::string& message) {
  log_error(std::string("Showing error dialog: ") + message);
  Gtk::MessageDialog dialog(*this, message, false, Gtk::MESSAGE_ERROR,
                            Gtk::BUTTONS_OK, true);
  dialog.run();
}

}  // namespace tv
