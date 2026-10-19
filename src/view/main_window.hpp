#pragma once

#include "view/channel_list.hpp"
#include "view/playback_bar.hpp"

#include <memory>

#include <gtkmm.h>

namespace tv {

class MainPresenter;

class MainWindow : public Gtk::ApplicationWindow {
 public:
  MainWindow();
  ~MainWindow() override;

  ChannelListPane& channels() { return *channels_; }
  PlaybackBar& controls() { return *controls_; }
  Gtk::EventBox& video_area() { return video_area_; }
  Gtk::MenuBar& menu_bar() { return menu_bar_; }

  void set_chrome_visible(bool visible);
  void show_error(const std::string& message);

 private:
  void build_ui();
  Gtk::MenuBar* build_menu_bar();

  Gtk::MenuBar menu_bar_;
  ChannelListPane* channels_ = nullptr;
  PlaybackBar* controls_ = nullptr;
  Gtk::EventBox video_area_;
  std::unique_ptr<MainPresenter> presenter_;
};

}  // namespace tv
