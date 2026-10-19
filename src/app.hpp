#pragma once

#include <memory>

#include <gtkmm.h>

namespace tv {

class MainWindow;

class TvApplication : public Gtk::Application {
 public:
  static constexpr const char* kAppId = "com.project.tv.cpp";

  TvApplication();

 protected:
  void on_startup() override;
  void on_activate() override;

 private:
  std::unique_ptr<MainWindow> window_;
};

int run_app(int argc, char** argv);

}  // namespace tv
