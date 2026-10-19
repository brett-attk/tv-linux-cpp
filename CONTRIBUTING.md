# Contributing

## Development setup

Follow the [Quick start](README.md#quick-start) in the README to install
dependencies and build.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./run.sh
```

## Code layout

- **Model** (`src/model/`): persistence, HDHomeRun client — no GTK
- **View** (`src/view/`): gtkmm widgets; emit callbacks only
- **Presenter** (`src/presenter/`): wires store, UI, and player
- **Infra** (`src/infra/`): paths, logging, libmpv wrapper, icons, theme

Prefer matching existing style: C++17, focused changes, no drive-by refactors.

## Tests

Add or extend headless tests under `tests/` for model/infra behavior
(URL normalization, channel store, settings). UI and mpv do not need unit tests
for routine changes.

## Static analysis

```bash
# cppcheck (warnings that indicate real bugs)
cppcheck --enable=warning,style,performance,portability --std=c++17 \
  --inline-suppr --suppress=missingIncludeSystem --suppress=unusedFunction \
  --suppress=useStlAlgorithm -I src src

# Optional: clang-tidy (requires compile_commands.json from CMake)
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
run-clang-tidy -p build 'src/.*\.cpp'
```

## Pull requests

1. Keep commits focused and messages imperative (“Add…”, “Fix…”)
2. Ensure `cmake --build build` and `ctest --test-dir build` pass
3. Do not commit `build/`, IDE files, or local config under `~/.config/tv-cpp/`
