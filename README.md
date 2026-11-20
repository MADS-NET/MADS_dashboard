# MADS Dashboard

A desktop GUI for watching live data from [MADS](https://github.com/pbosetti/mads) agents. It connects to a MADS broker, either by settings address or by room discovery, receives the JSON messages that agents publish, and plots them in dockable views. It is inspired by the layout of Rerun. It is written in C++20 with Dear ImGui (docking), ImPlot, ImPlot3D and GLFW/OpenGL 3.

The project is in early development. The view types are **Time series**, **Readout**, **XY**, **XYZ**, **Histogram**, **Spectrum**, **Boxplot** and **Heatmap**. See `PLAN.md` for the roadmap.

## Requirements

- **MADS v3**, installed. CI tests against the MADS release `v3.0.0-beta4`. The build finds MADS through `mads -p`, so the `mads` command must be on `PATH`. You can also point CMake at the install directly (see Build).
- **CMake 3.28** or newer, and a **C++20 compiler**.
- **Ninja** is recommended on macOS and Linux, as in CI. On Windows CI uses the default Visual Studio generator.
  - macOS: `brew install ninja`
  - Ubuntu: `sudo apt-get install -y ninja-build libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`
- CMake fetches the other dependencies (GLFW, Dear ImGui, ImPlot, ImPlot3D, HDF5, HighFive, doctest) at pinned tags and links them statically.
- **Runtime:** only `libMadsCore` from the MADS install. On Windows, `MadsCore.dll` (in `<prefix>/bin`) must be on `PATH`.

## Build

macOS and Linux:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

On Windows, use the default Visual Studio generator: `cmake -S . -B build`, then `cmake --build build --config Release`.

If CMake cannot find MADS, pass the install prefix (`<prefix>` is the output of `mads -p`). When `Mads_DIR` is set, CMake skips `mads -p`, so pass the prefix too:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<prefix> -DMads_DIR=<prefix>/lib/cmake/Mads
```

The first build also compiles HDF5 and the GUI libraries, so it is slow.
Options: `-DMADS_DASH_BUILD_TESTSOURCE=OFF` skips `tools/testsource` (ON by default; it builds into `build/testsource/`), and `-DMADS_DASH_BUILD_TESTS=OFF` skips the unit tests. The executable is `build/mads-dashboard`.

## Run

```bash
./build/mads-dashboard --help
```

The options are the standard MADS agent options. The ones you will use most:

| Option | Meaning |
|--------|---------|
| `-s, --settings URI` | Broker settings address, for example `tcp://localhost:9092` |
| `-r, --room [NAME]` | Join by room discovery (room name defaults to `mads`) |
| `--settings-timeout MS` | Timeout for reading settings from the broker. The dashboard adds `3000` when you do not pass it, so a missing broker does not hang the network thread |
| `--transport auto\|tcp\|ipc` | Transport to the broker (`auto` uses ipc when the broker is on this host) |
| `--crypto`, `--keys_dir`, `--key_broker`, `--key_client`, `--auth_verbose` | CURVE encryption |
| `-S, --save-settings FILE` | Save the settings to an INI file |
| `-n, --name`, `-i, --agent-id`, `-v`, `-h` | Agent name and ID, version, usage |

`topic` arguments are subscription filters. MQTT-style wildcards are allowed. With no topics, the settings decide.

```bash
./build/mads-dashboard -s tcp://localhost:9092 testsource signals
```

## Local test setup

The repository root contains a local setup: `mads.ini` and `director.toml`.

`mads.ini` defines:

- `[broker]`: frontend on `tcp://*:9090`, backend on `tcp://*:9091`, settings on `tcp://*:9092`.
- `[signals]`: a source every 100 ms (10 Hz) on topic `signals`, nested under `signals`. Fields: `temperature` (sine) and `pressure` (square wave plus noise). The `[rerunner]` section is a reference for subscriptions and is not used by the dashboard.
- `[testsource]`: the settings for `testsource.plugin`, described below.

`director.toml` starts `mads broker -d`, then `mads source signals.plugin`, then `mads source build/testsource/testsource.plugin` (the path the build produces). `signals.plugin` is not part of this repository. It must be installed where `mads` can find it (PLAN.md §3.4 refers to `mads-net/signals_plugin`).

From the repository root, either use the director:

```bash
mads director
```

or with three commands in three terminals:

```bash
mads broker -d
mads source signals.plugin
mads source build/testsource/testsource.plugin
```

Then start the dashboard:

```bash
./build/mads-dashboard -s tcp://localhost:9092 testsource signals
```

### What `testsource` publishes

`tools/testsource/src/testsource.cpp` publishes one message per tick on the topic `testsource`. Each message has these fields:

- `accel`: `[ax, ay, az]`, three sines at 1, 2 and 3 Hz plus small noise.
- `position`: `[x, y, z]`, a 3D Lissajous curve.
- `pose`: `[x, y, z, qw, qx, qy, qz]`, the same position with a rotation about z.
- `xy`: `{"x": [...], "y": [...]}`, `xy_points` samples of a sine that moves in time.
- `map`: a `matrix_rows` × `matrix_cols` 2D Gaussian whose centre moves on a circle. It is omitted when either size is 0.
- `state`: a string that cycles `idle` → `run` → `hold` every `state_period` seconds.
- `alarm`: a bool, true when `accel[0] > 0.9`.
- `counter`: an integer tick count.

Its settings, all optional, in `[testsource]`:

| Key | Default | Meaning |
|-----|---------|---------|
| `period` | 10 | Loop period in ms, consumed by the MADS host. `mads.ini` sets 10 |
| `xy_points` | 64 | Samples in `xy` |
| `matrix_rows`, `matrix_cols` | 32, 32 | Size of `map` |
| `state_period` | 2.0 | Seconds per `state` value |

The host sleeps the full period after each tick, so `period = 10` gives about 83 Hz, not 100 Hz (PLAN.md §12). A 90 × 90 matrix gives messages of about 47 kB; about 105 × 105 gives 64 kB.

## Using the GUI

- **File → Connect...** Choose *Broker settings URI* (for example `tcp://localhost:9092`) or *Room (discovery)*, and type the topics separated by spaces. An empty topic list means the settings decide. **File → Clear data** empties the store, and **File → Quit** exits.
- **File → Save layout... / Load layout...**: views, their sources and options, docking and the timeline window, as JSON. The last session is restored automatically at start-up and saved on exit (window close, Ctrl+C or SIGTERM) to `mads-dashboard/session.json` in the per-user config folder (`~/Library/Application Support` on macOS, `$XDG_CONFIG_HOME` or `~/.config` on Linux, `%APPDATA%` on Windows).
- **File → Export HDF5**: the whole session or the current timeline window. Each keypath becomes a group with `t` and `value` datasets and a `kind` attribute; matrices keep only their latest value.
- **Sources** (left panel): a tree of the keypaths seen so far, with the latest value of each field. Right-click a field to open its menu:
  - **New <type> view**, for each view type that accepts the field's kind;
  - **Add to <view>**, for each existing view that accepts the field.
  - A field with no compatible view shows "No view for this kind of data".
- **Views** (centre): each view is a window that can be docked. Its **Sources** menu removes a source, and its **Options** menu holds the view's settings.
  - *Time series* (numbers, bools, strings as event lines): Options → `cursor` adds a draggable cursor with `snap`, values at the cursor and **Copy**.
  - *Readout* (numbers, bools, strings, arrays): latest value and its age.
  - *XY* (numbers, arrays): two equal-length arrays, two numbers, or one `[x, y, ...]` array; `history`, `fade older`, `markers`, *Swap X / Y*.
  - *XYZ* (numbers, arrays): one `[x, y, z, ...]` array or three numbers, in 3D (drag to rotate); a 7-element `[x, y, z, qw, qx, qy, qz]` array also shows the latest pose as red/green/blue body axes; `history`, `fade older`, `markers`, `pose axes`, `equal scale` (on by default, so lengths and angles are true).
  - *Histogram* (numbers): values in the time window; `bins`, `density`.
  - *Spectrum* (numbers): amplitude spectrum (FFT) of the time window; `log Y`.
  - *Boxplot* (numbers): cumulative since creation or **Reset**.
  - *Heatmap* (matrices): latest matrix; `auto range` or `min`/`max`, colormap. It does not follow the timeline when paused.
- **Timeline** (bottom panel): **Pause/Resume** (or the Space key, when no text field has focus) freezes the view. While paused, the slider scrubs through everything received; all views follow. Use the *window* slider (1 to 600 s) to set the visible time span. While paused you can pan and zoom freely. While live, the timeline follows the newest data.
- **Menu bar:** connection state, message rate, a *dropped* counter under overload, frame rate and MADS version.

## Tests

```bash
./build/tests/dash_tests
ctest --test-dir build -C Release --output-on-failure
```

The test suite is in `tests/test_*.cpp`, using doctest. The live receiver tests are skipped unless `MADS_DASH_LIVE=1` is set. They need the local setup running (broker on `tcp://localhost:9092` and `testsource`):

```bash
MADS_DASH_LIVE=1 ./build/tests/dash_tests
```

CI (`.github/workflows/ci.yml`) builds and runs `ctest` on macOS, Ubuntu and Windows.

## Project documents and license

`PLAN.md` holds the requirements, architecture and roadmap; `TASKS.md` holds the task board. License: TBD.
