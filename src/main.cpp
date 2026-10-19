#include "app.hpp"
#include "infra/app_icon.hpp"
#include "infra/logging.hpp"

#include <clocale>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  // Prefer X11 so mpv can embed via window id (needed under Wayland).
  setenv("GDK_BACKEND", "x11", 0);
  // libmpv requires the C numeric locale.
  setenv("LC_NUMERIC", "C", 1);
  setlocale(LC_NUMERIC, "C");

  const auto log_file = tv::setup_logging();
  tv::log_info("Starting tv-cpp (error log: " + log_file.string() + ")");

  try {
    // Default icon is applied after GTK init inside run_app via startup.
    return tv::run_app(argc, argv);
  } catch (const std::exception& exc) {
    tv::log_exception(std::string("tv-cpp exited with an unhandled error: ") +
                      exc.what());
    std::cerr << "Fatal: " << exc.what() << '\n';
    return 1;
  }
}
