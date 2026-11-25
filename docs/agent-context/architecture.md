# Architecture

```
 MADS broker ──ZMQ──▶ net::Receiver (own thread, Mads::AgentApp sink)
                              │  bounded deque (100k msgs, drop oldest, count)
                              ▼
            UI thread, once per frame: drain → data::Store::ingest (flatten)
                              │  Store has a single owner: no locks
                              ▼
            ui::AppWindow: Sources tree, Timeline, Views (ImGui + ImPlot/ImPlot3D)
```

`mads play` (bag → broker) feeds the same path: the dashboard has no bag I/O of its own.

## Source layout

| Path | Contents |
|------|----------|
| `src/app/main.cpp` | CLI: `Receiver::handle_cli` (MADS options, `--help`), `--stats`, `--layout`, `--verbose`; `detach()` (skipped with `--verbose`, which runs in the foreground) re-launches the program as a detached child (posix_spawn + setsid / CreateProcess, marked by env `MADS_DASHBOARD_DETACHED`; plain fork() crashes on macOS because the ObjC runtime is already loaded); SIGINT/SIGTERM handlers. |
| `src/net/receiver.{hpp,cpp}` | `Receiver` (network thread), `ConnectForm` + `connect_args()` (Connect dialog → CLI args), `missing_key_files()`, `default_keys_dir()`. |
| `src/data/store.{hpp,cpp}` | `Store`, `Series`, `Kind`, `flatten()`, `TimeBase`, `timecode_to_utc()`, `parse_iso8601()`, `local_utc_offset()`. |
| `src/data/export_hdf5.{hpp,cpp}` | `export_hdf5()`: Store → HDF5 file (HighFive). |
| `src/ui/app_window.{hpp,cpp}` | Window, main loop, menus, Connect dialog, Sources tree, Timeline panel, view windows, session/layout/export actions, `--stats`. |
| `src/ui/view.{hpp,cpp}` | `View` base class, `Timeline`, `ViewType` registry (`view_types()`). |
| `src/ui/views/*_view.{hpp,cpp}` | One file pair per chart type (globbed by CMake). |
| `src/ui/auto_scale.{hpp,cpp}` | `StickyRange`, `AutoScale` (shared plot interaction). |
| `src/ui/layout.{hpp,cpp}` | Layout/session JSON (de)serialization, `session_file()`. |
| `src/ui/style.{hpp,cpp}` | MADS GUI look (copied from MADS `src/gui/common/gui_style.cpp`, which MADS does not install). |
| `src/ui/im_assert.cpp`, `imconfig_dash.h` | Keeps ImGui/ImPlot asserts active in Release: logged once by the app, thrown in tests. |
| `tests/test_*.cpp` | doctest suite (globbed). `tests/consumer/`: stand-in for the MADS build consuming the dashboard. |
| `tools/testsource/` | `testsource.plugin`, a MADS source publishing every data shape (see its README). |

Namespaces: `mads_dash::{app,net,data,ui}`.

## Network: `net::Receiver`

- Built on MADS's own `Mads::AgentApp`, so the dashboard takes the standard MADS agent options (`-s`, `--room`, `--crypto`, key options, `-n`, topics) and MADS does the parsing, discovery and CURVE. GUI reconnects build the same argument list (`connect_args()`) and create a new `Receiver`.
- Agent name `dashboard`; settings section `[dashboard]`; `add_default_section()` subscribes to everything when the section is missing. Publishes nothing (`set_pub_topic("")`).
- `--settings-timeout 3000` is appended unless given (no broker must not hang the thread); receive timeout 100 ms so the stop flag is checked.
- `handle_cli()` is called once in `main()` to validate options and handle `--help`/`--version` (AgentApp's `parse_options` **exits the process** on bad options, so the thread must only ever see valid args).
- State `Connecting/Connected/Failed/Stopped` + status text for the top bar; `dropped()` counts overload drops.

## Data: `data::Store`

- **Keypaths:** `flatten()` maps a message to leaves: objects recurse with `/key`; numeric arrays and numeric matrices are leaves (arrays of ≤ 16 elements also yield `[i]` leaves); other arrays recurse with `[i]`; null/empty are skipped. `/topic/timestamp` as a plain ISO string is skipped; `…/$date` leaves (MADS v3 `timestamp: {"$date": …}`) are stored as **numbers** (UTC seconds), not strings.
- **Kinds** (first value wins): Number, Bool, String, Array, Matrix. `Series` holds `t`/`v` vectors (Number, Bool as 0/1), `events` (String, only on change), `snaps` (Array snapshots), `last`/`last_t` (all; Matrix keeps only `last`).
- **Time:** every sample time is **UTC epoch seconds** (double). Per topic (`TimeBase`):
  - default: MADS `timecode` = seconds since **local** midnight (MADS `timecode()` uses `localtime`), converted with the dashboard's own UTC offset (`local_utc_offset()`, or `set_fixed_utc_offset()` in tests) and the receive date, with ±12 h day-wrap correction; no timecode → receive time;
  - custom field (`set_time_field(topic, keypath)`): a Number (taken as seconds; `clock = true` if > 1e8, i.e. epoch-like) or a `…/$date` (ISO 8601 string with optional offset, or ms number). Choosing the topic's own `timecode` leaf means "default". Switching clears that topic's series and re-derives first/latest time (`recompute_times()`). Messages without the field are skipped.
  - the active time field stays a normal series (the tree shows it greyed, `(time scale)`); `is_time_field()`.
- **Retention:** `set_retention(seconds)` + `trim()` (called once a second by the app): drops samples older than retention **before the newest sample of their own topic** (per topic, so a topic on another clock is not wiped). `0` = unlimited. Optional hard caps per keypath (constructor) are off by default.
- `memory_bytes()` is an estimate (shown in the top bar).
- UI-thread only. **Never mutate the store while iterating `series()`** (the Sources tree defers time-base changes to after its loop: `_pending_time`).

## Timeline and views

- `ui::Timeline { paused, window, now, first }`: one timeline for all views. Live: `now` = store's latest time; paused: `now` is scrubbed. The common window is `[tl.begin(), tl.now]`, where `begin()` = `now - window`, or `first` when `window == 0` ("all").
- `View` (abstract): `type_name`, `accepts(Kind)`, `draw(store, tl)`, `draw_options()` (Options menu), `options()`/`set_options()` (JSON, for layouts; ignore missing/mistyped keys), `add_source`/`remove_source`, `title()` = `"<type> #<id>###view<id>"` (the `###` id keeps docking stable across renames and sessions).
- Registry: `view_types()` in `src/ui/view.cpp`, one line per type, in menu order. The Sources tree offers "New <type> view" for every type whose `accepts()` matches. Adding a view type = new file pair in `src/ui/views/` + one registry line + an entry in `tests/test_layout.cpp`'s `non_default()` (the layout test fails otherwise, deliberately).
- Pattern for a view: put the computation in a free, ImGui-free function declared in the header (unit-testable); `draw()` computes data first, updates the `AutoScale` ranges, then plots.
- Trace views (XY, XYZ) share a series model in `xy_view.hpp`: `AxisSource{key,index}`, `TraceSeries`, `trace_points()` (samples where every axis has a value with the same time), `sync_series()` (auto-assigns new sources to series; a `seen` set stops re-assigning sources the user moved), per-series Options UI.
- XY curve (`curve_view`): latest pair of equal-time array snapshots; `curve_pair()` finds a node with exactly two equal-length array children (used by the tree's node menu).

## Plot interaction: `AutoScale`

- `StickyRange::update(min, max)`: when data leave the range, it becomes `[min - r/2, max + r/2]` (r = max - min; zero width widens by |value| or 1), then stays put.
- `AutoScale` per plot: on by default; `begin_frame()` before `BeginPlot` applies last frame's mouse input on the plot: double-click → on (and reset ranges); left-drag, right-drag (box zoom; except 3D, where right-drag rotates: `begin_frame(true)`) or wheel → off. `set_hovered()` is called inside the plot (2D: `ImPlot::IsPlotHovered()` or axis hovered; 3D: `ImGui::IsWindowHovered()` after `EndPlot`). Draggable tools (the time-series cursor) must exclude their own drags.
- When on: `SetupAxes` **without** `ImPlotAxisFlags_AutoFit`, and `SetupAxisLimits(…, ImPlotCond_Always)`. When off: don't touch the limits (user pans/zooms).

## Layouts and session

- JSON `{format: 1, window, buffer, imgui_ini, views: [{type, id, sources, options}]}`; `layout_from_json()` never throws on bad content (warnings instead).
- ImGui's own ini file is disabled (`io.IniFilename = nullptr`); the docking state travels as `imgui_ini` inside the layout/session, applied before the next frame (`_pending_ini`, must be outside a frame).
- Session: restored at start (unless `--layout FILE`), saved on exit to `<config dir>/mads-dashboard/session.json` (macOS `~/Library/Application Support`, Linux `$XDG_CONFIG_HOME` or `~/.config`, Windows `%APPDATA%`). Clean exit on window close, File → Quit, SIGINT, SIGTERM (handlers in `main()` set MADS's stop flag; the render loop checks `mads_process_running()`).

## Build

- `CMakeLists.txt` + `cmake/Dependencies.cmake`. Two modes:
  - **standalone**: `find_package(Mads)` (prefix from `mads -p`), tests and `testsource` on, default install prefix = MADS prefix;
  - **inside the MADS build** (detected by `TARGET Mads::Mads`): uses MADS's in-tree `Mads::Mads` + `MadsMainDeps` (MADS's header deps are linked PRIVATE to MadsCore, so `MadsMainDeps` is needed) and MADS's `glfw`; tests/testsource off; installs only `bin/mads-dashboard` into `MADS_DASH_INSTALL_COMPONENT` (MADS: `MadsGui`). `tests/consumer/` checks this mode.
- Dependencies via FetchContent at pinned tags, all statically linked: GLFW 3.4 (named `glfw`, shared with MADS), and under `mads_dash_*` names Dear ImGui `v1.92.9b-docking`, ImPlot `v1.1`, ImPlot3D `v0.5`, portable-file-dialogs (pinned commit `c12ea8c…`, see decisions.md), HDF5 `2.2.0` (C only, static) + HighFive `v3.3.0`, doctest `v2.4.12` (tests). nlohmann/json, toml++, cppzmq, cxxopts come from MADS.
- Generic switches for dependencies (`BUILD_SHARED_LIBS`, `BUILD_TESTING`, `GLFW_*`) are **normal variables** scoped to our directory (with `CMAKE_POLICY_DEFAULT_CMP0077 NEW`), never forced cache entries: forcing them would make MadsCore static and switch MADS's tests off when built inside MADS.
- Runtime: `libMadsCore` only; installed binaries find it via a relative rpath (`@loader_path/../lib`, `$ORIGIN/../lib`); on Windows `MadsCore.dll` is next to the exe in `<prefix>/bin`.
- CI: `.github/workflows/ci.yml`, macOS/Linux/Windows, installs the pinned MADS release tarball (`v3.0.0-beta4`), builds, runs ctest. No packaging, no consumer test (user decisions).
