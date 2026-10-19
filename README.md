# linux-tv-cpp

GTK 3 (gtkmm) + libmpv television front-end for Linux. Plays HDHomeRun-style
HTTP MPEG-TS streams (for example `http://192.168.1.27:5004/auto/v2.1`).

Native C++ binary with an MVP architecture. Licensed under the [MIT License](LICENSE).

## Features

- Channel list with friendly names; right-click for Favorite / Edit / Remove
- Favorites sort to the top and show a heart icon
- Add channels manually or import from an HDHomeRun device lineup
- Embedded mpv video pane
- Prev / Next, volume, mute, fullscreen, OSC toggle (persisted)
- Local-only persistence under XDG paths (nothing is shipped in the repo)

## Requirements

| Dependency | Debian/Ubuntu package |
|------------|------------------------|
| CMake ≥ 3.16, C++17 compiler | `cmake` `g++` `pkg-config` |
| gtkmm 3 | `libgtkmm-3.0-dev` |
| libmpv | `libmpv-dev` (runtime: `libmpv2`) |
| libcurl | `libcurl4-openssl-dev` |
| nlohmann/json | `nlohmann-json3-dev` |

Video embedding uses X11 window ids. On Wayland, launch with `GDK_BACKEND=x11`
(or use `./run.sh`).

## Quick start

```bash
git clone https://github.com/brett-attk/linux-tv-cpp.git
cd linux-tv-cpp

sudo apt update
sudo apt install cmake g++ pkg-config libgtkmm-3.0-dev libmpv-dev \
  libcurl4-openssl-dev nlohmann-json3-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure

./run.sh
```

On first launch the channel list is empty. Use **Settings → Add Channel…** or
**Settings → Import from HDHomeRun…**.

### Optional system install

```bash
sudo cmake --install build
```

This installs the `tv-cpp` binary, hicolor icons, and a desktop entry
(`tv-cpp.desktop`).

## Run

```bash
./run.sh
# or
GDK_BACKEND=x11 ./build/tv-cpp
```

## Controls

| Action | UI | Keys |
|--------|----|------|
| Play / Stop | Play icon on selected row | — |
| Add channel | Settings → Add Channel… | — |
| Import HDHomeRun | Settings → Import from HDHomeRun… | — |
| Favorite / Edit / Remove | Right-click channel | — |
| Previous / next | Prev / Next | Page Up / Page Down |
| Volume | Vol − / Vol + | `-` / `+` |
| Mute | Mute | `m` |
| OSC | OSC toggle | — |
| Fullscreen | Fullscreen | `f` / Esc to exit |

### HDHomeRun import

1. **Settings → Import from HDHomeRun…**
2. Enter the device address (`192.168.1.27`, `http://hdhomerun.local`, or a full `lineup.json` URL)
3. **Fetch** → select channels → **Add selected**

Channels already in your list (same stream URL) are marked and skipped.

## Where data is stored

Nothing from your channel list is committed to git. On first launch the app
creates config/state files as needed.

| What | Path |
|------|------|
| Channel list + favorites | `~/.config/tv-cpp/channels.json` |
| App settings (OSC) | `~/.config/tv-cpp/settings.json` |
| Error log | `~/.local/state/tv-cpp/error.log` |

Override with `XDG_CONFIG_HOME` / `XDG_STATE_HOME` if needed.

Example `channels.json`:

```json
{
  "channels": [
    {
      "id": "...",
      "name": "Channel 2.1",
      "url": "http://192.168.1.27:5004/auto/v2.1",
      "favorite": false
    }
  ]
}
```

## Project layout

```
src/
  main.cpp              entry (logging + GDK_BACKEND)
  app.*                 Gtk::Application
  presenter/            MainPresenter (MVP coordinator)
  model/                channels, settings, HDHomeRun client
  view/                 gtkmm widgets and dialogs
  infra/                paths, logging, mpv player, theme, icons
data/
  icons/                application icon (tv-cpp)
  tv-cpp.desktop.in     desktop entry template
tests/                  unit tests (no display / no libmpv required)
```

## Tests

```bash
ctest --test-dir build --output-on-failure
# or
./build/tests/tv_core_tests
```

## Troubleshooting

| Problem | What to check |
|---------|----------------|
| Missing gtkmm / mpv / curl / json | Install the packages under [Requirements](#requirements) |
| Black video / no picture | Launch with `./run.sh` (forces X11). Plain Wayland without X11 embedding will not work |
| Import / network errors | See `~/.local/state/tv-cpp/error.log` |
| Icon missing when running from `build/` | Rebuild so `TV_CPP_ICON_DIR` points at `data/icons`, or install |

## License

[MIT](LICENSE)
