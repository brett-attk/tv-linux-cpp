#include "app.hpp"

#include "infra/app_icon.hpp"
#include "infra/theme.hpp"
#include "view/main_window.hpp"

namespace tv {

TvApplication::TvApplication()
    : Gtk::Application(kAppId, Gio::APPLICATION_FLAGS_NONE) {}

void TvApplication::on_startup() {
  Gtk::Application::on_startup();
  init_color_scheme();
  apply_app_icon();
}

void TvApplication::on_activate() {
  if (!window_) {
    window_ = std::make_unique<MainWindow>();
    add_window(*window_);
  }
  window_->present();
}

int run_app(int argc, char** argv) {
  TvApplication app;
  return app.run(argc, argv);
}

}  // namespace tv
