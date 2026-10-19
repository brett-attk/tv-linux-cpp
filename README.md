# TV (C++)

GTK 3 + libmpv television front-end for Linux. Plays HDHomeRun-style HTTP
MPEG-TS streams (for example `http://192.168.1.27:5004/auto/v2.1`).

**Version:** see [`VERSION`](VERSION) (current: **0.1.0**). Shown in the app under
**Settings → About**, which also links to this repository:
https://github.com/brett-attk/tv-linux-cpp

Licensed under the [MIT License](LICENSE).

## Features

- Left pane: channel list with friendly names; right-click for Favorite / Edit / Remove
- Favorites sort to the top of the list and show a heart icon
- Settings menu: add a channel or import from an HDHomeRun device lineup
- Right pane: embedded mpv video
- Channel Prev / Next, volume, mute, fullscreen
- Local-only persistence under XDG paths (see [Where data is stored](#where-data-is-stored))

## Setup and install (Debian/Ubuntu)

Do these steps once, from a terminal.

### 1. Get the code

```bash
git clone https://github.com/brett-attk/tv-linux-cpp.git
cd tv-linux-cpp
```

### 2. Install system packages

```bash
sudo apt update
sudo apt install cmake g++ pkg-config libgtkmm-3.0-dev libmpv-dev \
  libcurl4-openssl-dev nlohmann-json3-dev
```

These provide the C++ compiler, GTK 3 UI toolkit, libmpv (video), libcurl
(network), and JSON support.

### 3. Build the app

From the project directory (`tv-linux-cpp`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This creates the binary at `build/tv-cpp`.

Optional — run the unit tests:

```bash
ctest --test-dir build --output-on-failure
```

### 4. Run the app

```bash
./run.sh
```

`run.sh` starts the app with X11 embedding enabled (needed for video on Wayland).

On first launch the channel list is empty. Add channels with
**Settings → Add Channel…** or **Settings → Import from HDHomeRun…**.
Your list is saved only on this machine (see [Where data is stored](#where-data-is-stored)).

**Later launches:** from the project directory, run `./run.sh` again
(no reinstall needed).

**Alternative launch** (same as `./run.sh`):

```bash
GDK_BACKEND=x11 ./build/tv-cpp
```

### 5. Optional: install system-wide

Only if you want a Start Menu / app launcher entry and `tv-cpp` on your PATH:

```bash
sudo cmake --install build
```

Then you can open **TV** from your desktop menu, or run:

```bash
GDK_BACKEND=x11 tv-cpp
```

## Where data is stored

Nothing from your channel list is shipped in the git repo. On first launch the
app creates the config directory and empty files as needed.

| What | File | Notes |
|------|------|--------|
| **Channel list + favorites** | `~/.config/tv-cpp/channels.json` | Updated when you add, edit, remove, import, or favorite a channel |
| **App settings** (e.g. OSC on/off) | `~/.config/tv-cpp/settings.json` | Preferences that survive restarts |
| **Error log** | `~/.local/state/tv-cpp/error.log` | Rotating log for import/network and other failures |

```bash
# Inspect your channel list
less ~/.config/tv-cpp/channels.json

# Inspect settings
less ~/.config/tv-cpp/settings.json

# Inspect the error log
less ~/.local/state/tv-cpp/error.log
```

To relocate storage, set `XDG_CONFIG_HOME` and/or `XDG_STATE_HOME` before launching.

## Controls

| Action | UI | Keys |
|--------|----|------|
| Play / Stop | Play icon on selected row | — |
| Add channel | Settings → Add Channel… | — |
| About | Settings → About | — |
| Favorite / Edit / Remove | Right-click channel | — |
| Import HDHomeRun | Settings → Import from HDHomeRun… | — |
| Previous / next | Prev / Next | Page Up / Page Down |
| Volume | Vol − / Vol + | `-` / `+` |
| Mute | Mute | `m` |
| OSC | OSC toggle | — |
| Fullscreen | Fullscreen | `f` / Esc to exit |

### HDHomeRun import

1. Open **Settings → Import from HDHomeRun…**
2. Enter the device address (examples: `192.168.1.27`, `http://hdhomerun.local`, or `http://192.168.1.27/lineup.json`)
3. Click **Fetch** to load the lineup
4. Check the channels to add, then **Add selected**

Channels already in your list (same stream URL) are marked and skipped.

## Project layout

```
src/
  main.cpp              entry (logging + GDK_BACKEND)
  app.*                 Gtk::Application
  presenter/            MainPresenter (MVP coordinator)
  model/                channels, settings, HDHomeRun client
  view/                 gtkmm widgets and dialogs
  infra/                paths, logging, mpv player, theme, icons
data/                   application icon and desktop entry
tests/                  unit tests (no display required)
```

## Performance smoke test

Optional check for launch time, memory (RSS), and playback FPS under Xvfb
(uses a local ffmpeg MPEG-TS test stream — no HDHomeRun needed):

```bash
sudo apt install ffmpeg xvfb
./scripts/perf_smoke.sh
```

Or: `cmake --build build --target perf-smoke`. See [CONTRIBUTING.md](CONTRIBUTING.md)
for thresholds and override env vars. This is **not** part of normal `ctest`.

## Troubleshooting

| Problem | What to check |
|---------|----------------|
| `cmake` or package not found | Re-run step 2 (install system packages) |
| Build errors about gtkmm / mpv / curl / json | Same — install the packages in step 2, then rebuild (step 3) |
| Black video / no picture | Launch with `./run.sh` (forces X11). Plain Wayland without X11 embedding will not work |
| Import / network errors | Details are written to `~/.local/state/tv-cpp/error.log` |

```bash
# View the error log
less ~/.local/state/tv-cpp/error.log
```

## License

[MIT](LICENSE)
