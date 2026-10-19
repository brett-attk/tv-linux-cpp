cmake_minimum_required(VERSION 3.16)

# Single source of truth for the app version (also shown in Settings → About).
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TV_CPP_VERSION_FILE)
string(STRIP "${TV_CPP_VERSION_FILE}" TV_CPP_VERSION_FILE)
project(tv-cpp VERSION ${TV_CPP_VERSION_FILE} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-3.0)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_library(tv_core STATIC
  src/infra/paths.cpp
  src/infra/logging.cpp
  src/model/channel_store.cpp
  src/model/settings.cpp
  src/model/hdhomerun.cpp
)
target_include_directories(tv_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tv_core PUBLIC nlohmann_json::nlohmann_json PkgConfig::CURL)
target_compile_definitions(tv_core PUBLIC TV_CPP_VERSION="${PROJECT_VERSION}")

add_library(tv_player STATIC
  src/infra/mpv_player.cpp
)
target_include_directories(tv_player PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tv_player PUBLIC tv_core PkgConfig::MPV)

add_executable(tv-cpp
  src/main.cpp
  src/app.cpp
  src/presenter/main_presenter.cpp
  src/view/main_window.cpp
  src/view/channel_list.cpp
  src/view/playback_bar.cpp
  src/view/dialogs.cpp
  src/view/import_dialog.cpp
  src/infra/theme.cpp
  src/infra/app_icon.cpp
  src/infra/perf.cpp
)
target_include_directories(tv-cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tv-cpp PRIVATE tv_core tv_player PkgConfig::GTKMM)
# X11 wid embedding
find_package(X11 REQUIRED)
target_link_libraries(tv-cpp PRIVATE X11::X11)
target_compile_definitions(tv-cpp PRIVATE
  TV_CPP_VERSION="${PROJECT_VERSION}"
  TV_CPP_ICON_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/icons"
)
install(TARGETS tv-cpp RUNTIME DESTINATION bin)

# Icons + desktop entry for shippable installs
include(GNUInstallDirs)
foreach(size IN ITEMS 32 48 64 128 256)
  install(FILES data/icons/tv-cpp-${size}.png
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
          RENAME tv-cpp.png)
endforeach()
install(FILES data/icons/tv-cpp.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
        RENAME tv-cpp.png)
configure_file(data/tv-cpp.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/tv-cpp.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tv-cpp.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

enable_testing()
add_subdirectory(tests)

# Optional performance smoke (skipped when ffmpeg/xvfb/binary are missing).
add_custom_target(perf-smoke
  COMMAND ${CMAKE_COMMAND} -E env
          "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}"
          bash "${CMAKE_SOURCE_DIR}/scripts/perf_smoke.sh"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS tv-cpp
  COMMENT "Running launch/RSS/FPS performance smoke under Xvfb"
  USES_TERMINAL
)
