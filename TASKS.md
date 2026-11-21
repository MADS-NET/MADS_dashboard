# MADS Dashboard — Task board

Maintained by the coordinator. Protocol: PLAN.md §10.
Status: `todo → assigned → review → done` (or `rework`, `escalated`).

| ID | Title | Owner | Status | Branch |
|----|-------|-------|--------|--------|
| T-0.1 | Build skeleton: CMake, dependencies, empty docked window linked to MadsCore | coordinator | done | master |
| T-0.2 | `testsource.plugin`: arrays, pose, XY, matrix, events | sonnet | done (merged cc44717) | — |
| T-0.3 | CI (GitHub Actions, 3 OSes, MADS release tarball on runners) | sonnet | done (green on 3 OSes) | — |
| T-0.4 | README: build instructions + local test setup | haiku | todo (after T-0.1, T-0.2) | — |

---

## T-0.2 — `testsource.plugin`

- **Goal:** a MADS **source** plugin that publishes every data shape the dashboard must handle (arrays, pose, XY arrays, matrix, strings/bools), at a configurable rate and payload size.
- **Context:** PLAN.md §3.2, §3.4. MADS plugin skill: `~/usr/local/share/skills/mads-plugin/` (SKILL.md.tpl + reference/*.md). Template: `~/usr/local/share/templates/source.cpp`. Existing example config: `mads.ini` `[signals]`.
- **Contract:**
  - Scaffold with `mads plugin testsource -t source -d tools/testsource`, then implement. It's a standalone CMake project (as generated), not part of the top-level build.
  - Plugin name / driver / `kind()`: `testsource`. Settings section: `[testsource]`.
  - Settings (all optional, defaults in brackets):
    - `period` [10] ms (the host's loop period);
    - `xy_points` [64];
    - `matrix_rows` [32], `matrix_cols` [32] (0 for either disables `map`);
    - `state_period` [2.0] s.
  - Time `t` = seconds since `set_params()` (steady clock). Every message contains these top-level fields:
    - `accel`: `[ax, ay, az]` — 3 sines at 1, 2, 3 Hz, amplitude 1, plus small gaussian noise;
    - `position`: `[x, y, z]` — 3D Lissajous `x=sin(0.3t)`, `y=sin(0.4t+π/2)`, `z=0.5 sin(0.5t)`;
    - `pose`: `[x, y, z, qw, qx, qy, qz]` — same position; unit quaternion for a rotation about z by angle `0.2t`;
    - `xy`: `{"x": [...], "y": [...]}` — `xy_points` samples, `x` uniform on [0, 2π], `y = sin(x + t)`;
    - `map`: `[[...], ...]` — `matrix_rows`×`matrix_cols` 2D gaussian whose centre moves on a circle (period 10 s); omitted when disabled;
    - `state`: string cycling `"idle" → "run" → "hold"` every `state_period` s;
    - `alarm`: bool, true when `accel[0] > 0.9`;
    - `counter`: integer tick count.
  - The plugin must not block or sleep (the host drives the loop). Exceptions must never escape: set `_error` and return `error`.
  - `main()` (direct execution) prints 3 consecutive outputs with default params.
- **May modify:** `tools/testsource/**` (new), and **append only** a `[testsource]` section to `mads.ini` and a `[testsource]` process to `director.toml` (`command = "mads source tools/testsource/build/testsource.plugin"`, `after = "broker"`).
- **Must not:** touch PLAN.md, TASKS.md, any other existing line in `mads.ini`/`director.toml`, or any file outside `tools/testsource/`. No dependencies beyond what the scaffold fetches plus the C++ standard library.
- **Acceptance:**
  1. `cmake -S tools/testsource -B tools/testsource/build -G Ninja && cmake --build tools/testsource/build` succeeds with no warnings from plugin code.
  2. Running the built plugin directly prints 3 JSON outputs with every field present and the right shapes (`map` 32×32, `xy.x`/`xy.y` 64 each, `pose` 7 elements, quaternion norm 1 ± 1e-9).
  3. Live: start `mads broker -d` (from the repo root, which uses `mads.ini`), then `mads source tools/testsource/build/testsource.plugin`. Then `mads echo --jsonl --count 50 testsource` receives 50 messages with all fields, at about 100 Hz. Stop all processes afterwards.
  4. With `matrix_rows = matrix_cols = 90`, messages are around 64 kB and are still received.
  5. The code passes `clang-format` with LLVM style.
- **Size:** about 250 lines of plugin code.

---

## T-0.3 — CI

- **Goal:** GitHub Actions workflow that builds the project and runs the tests on macOS, Linux (x86_64) and Windows on every push to `master` and on every pull request.
- **Context:** `CMakeLists.txt`, `cmake/Dependencies.cmake`, `tests/CMakeLists.txt`. MADS is a required, *installed* dependency, found with `find_package(Mads)`. On a runner, install it from the pinned MADS GitHub release **`v3.0.0-beta4`** (repo `pbosetti/mads`), using the tarballs `Mads-v3.0.0-beta4-Darwin-universal.tar.gz`, `Mads-v3.0.0-beta4-Linux-x86_64_headless.tar.gz` (or the non-headless one if the headless one lacks the SDK) and `Mads-v3.0.0-beta4-Windows-AMD64.zip`. Download them with `gh release download` (the `GH_TOKEN` env var comes from `github.token`) and extract them into a prefix. Then point CMake at that prefix with `-DCMAKE_PREFIX_PATH=<prefix>` and `-DMads_DIR=<prefix>/lib/cmake/Mads` (the top-level CMake skips `mads -p` when `Mads_DIR` is set). For example, MADS's own CI is in `.github/workflows/ci.yml` of `pbosetti/mads`.
- **Contract:**
  - One file: `.github/workflows/ci.yml`. Matrix: `macos-latest`, `ubuntu-latest`, `windows-latest`. Release build, Ninja on macOS and Linux, the default VS generator on Windows.
  - Linux: install the GLFW X11 and OpenGL dev packages (`libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`).
  - Cache the FetchContent sources (`build/_deps/*-src` or `FETCHCONTENT_BASE_DIR`), keyed on the hash of `cmake/Dependencies.cmake` and `tests/CMakeLists.txt`.
  - Steps: install MADS → configure (`-DMADS_DASH_BUILD_TESTSOURCE=ON`) → build → `ctest --output-on-failure`. At runtime the tests need `libMadsCore` on the library path: `DYLD_LIBRARY_PATH` / `LD_LIBRARY_PATH` / `PATH` on Windows.
- **May modify:** `.github/workflows/ci.yml` (new). If, and only if, a build fix is unavoidable for Windows or Linux: the smallest possible change to `CMakeLists.txt` or `cmake/Dependencies.cmake`, explained in the report.
- **Must not:** push, add remotes, or touch any other file.
- **Acceptance:** (1) the archive layout of every tarball is checked locally (download, then `tar -tzf`/`unzip -l`), and the paths in the workflow match it (the `lib/cmake/Mads` location and the shared library location); (2) the YAML parses (`python3 -c "import yaml,sys; yaml.safe_load(open(sys.argv[1]))" .github/workflows/ci.yml`); (3) `actionlint` passes if available (no install required); (4) the local macOS build still passes with the `-DMads_DIR=...` style configure against the extracted Darwin tarball. Final verification on GitHub happens after the coordinator pushes.
- **Size:** about 80 lines of YAML.

---

# M2 — Views

| ID | Title | Owner | Status | Branch |
|----|-------|-------|--------|--------|
| T-2.0 | Groundwork: view registry, options menu, array snapshots, XY reference view, headless view tests | coordinator | done (d89f9b0) | master |
| T-2.1 | Readout view | sonnet | done | — |
| T-2.2 | Histogram view | sonnet | done | — |
| T-2.3 | Spectrum (FFT) view | sonnet | done | — |
| T-2.4 | Heatmap view | sonnet | done | — |
| T-2.5 | Cumulative boxplot view | sonnet | done | — |
| T-2.6 | Time-series cursor: snap, values, copy | sonnet | done | — |
| T-0.4 | README: build, run, local test setup, CLI | haiku | done | — |

## Rules shared by T-2.1 … T-2.6

- **Read first:** `src/ui/view.hpp` (the `View` interface, `Timeline`, `ViewType` registry), `src/ui/views/xy_view.{hpp,cpp}` (**the reference view: copy its structure and style**), `src/ui/views/time_series_view.cpp`, `src/data/store.hpp` (what `Series` holds per `Kind`), and `tests/test_views.cpp` (headless harness).
- **Structure:** one header and one source file in `src/ui/views/` (globbed by CMake: no CMake edit). Register the type with **one line** in `view_types()` in `src/ui/view.cpp`, plus its `#include`, appended after the existing entries. Put the data computation in a free function declared in your header (namespace `mads_dash::ui`) that doesn't depend on ImGui, so it's unit-testable. `draw()` only calls it and plots the result.
- **Timeline semantics:** data used is limited to `t <= tl.now`. Views over a time window use `[tl.now - tl.window, tl.now]`. When `tl.paused` is true, don't force axis limits (the user pans/zooms); when live, auto-fit as the reference view does.
- **Tests:** a new file `tests/test_<name>.cpp` (globbed) with doctest cases for your computation function. The existing headless test in `tests/test_views.cpp` automatically renders your view for every keypath it accepts, live and paused, and **any ImGui/ImPlot assertion makes it fail**. Don't edit `tests/test_views.cpp`.
- **Build in your worktree:** configure once with the shared dependency sources to skip downloads:
  `cmake -S . -B build -G Ninja -DMADS_DASH_BUILD_TESTSOURCE=OFF -DFETCHCONTENT_SOURCE_DIR_GLFW=/Users/p4010/Develop/MADS_dashboard/build/_deps/glfw-src -DFETCHCONTENT_SOURCE_DIR_IMGUI=/Users/p4010/Develop/MADS_dashboard/build/_deps/imgui-src -DFETCHCONTENT_SOURCE_DIR_IMPLOT=/Users/p4010/Develop/MADS_dashboard/build/_deps/implot-src -DFETCHCONTENT_SOURCE_DIR_IMPLOT3D=/Users/p4010/Develop/MADS_dashboard/build/_deps/implot3d-src -DFETCHCONTENT_SOURCE_DIR_HDF5=/Users/p4010/Develop/MADS_dashboard/build/_deps/hdf5-src -DFETCHCONTENT_SOURCE_DIR_HIGHFIVE=/Users/p4010/Develop/MADS_dashboard/build/_deps/highfive-src -DFETCHCONTENT_SOURCE_DIR_DOCTEST=/Users/p4010/Develop/MADS_dashboard/build/_deps/doctest-src`
  then `cmake --build build` and `./build/tests/dash_tests`.
- **Must not:** edit any file outside your own two view files, your own test file and the registry lines in `src/ui/view.cpp`. No new dependencies. No changes to `View`, `Store` or `Timeline`; if you think one is needed, stop and report.
- **Acceptance (all cards):** (1) the build has no warnings from your files; (2) `./build/tests/dash_tests` passes entirely, including the headless render of your view; (3) your computation tests cover the cases listed in your card; (4) `clang-format --style=file` leaves your files unchanged (binary: `/Library/Developer/CommandLineTools/usr/bin/clang-format`). The coordinator checks the GUI by eye.
- **Size:** about 150 lines per view plus tests. Keep options minimal: only those listed in your card.

## T-2.1 — Readout view
- **Accepts:** Number, Bool, String, Array.
- **Shows:** a table with one row per source: keypath, latest value at or before `tl.now` (numbers with 6 significant digits, bools as true/false, strings as is, arrays as `[a, b, c]`, truncated to 8 elements with "…"), and the age `tl.now - t` in seconds. Use a large font scale (option `big`, default on: `ImGui::SetWindowFontScale(2)` for the value column only, or equivalent).
- **Computation to test:** `std::string readout_text(const data::Series &, double now)` returning the formatted value at or before `now` ("—" if none): numbers, bools, strings, arrays, truncation, `now` earlier than the first sample.

## T-2.2 — Histogram view
- **Accepts:** Number.
- **Shows:** histogram of each source's values in the window `[now - window, now]`, overlaid with transparency. Options: `bins` (int, 5 to 200, default 30), `density` (bool: normalise to a probability density).
- **Computation to test:** `std::vector<double> window_values(const data::Series &, double t0, double t1)`. Plotting can use `ImPlot::PlotHistogram`. Test: window bounds inclusive, empty window, unsorted input is not possible (series are time-sorted).

## T-2.3 — Spectrum (FFT) view
- **Accepts:** Number.
- **Shows:** single-sided amplitude spectrum of each source over the window `[now - window, now]`. Steps: resample to a uniform grid (linear interpolation) at the mean sample rate of the window, remove the mean, apply a Hann window, zero-pad to the next power of two, radix-2 FFT (write it, about 30 lines, no dependency), amplitude corrected for the Hann window. X axis frequency in Hz, Y amplitude. Options: `log Y` (bool, default off).
- **Computation to test:** `struct Spectrum { std::vector<double> f, amp; }; Spectrum spectrum(const std::vector<double> &t, const std::vector<double> &v)`. Test: a 5 Hz sine of amplitude 2 sampled at 100 Hz for 4 s gives a peak at 5 Hz ± one bin with amplitude 2 ± 10%; a constant gives a near-zero spectrum; fewer than 4 samples gives an empty result.

## T-2.4 — Heatmap view
- **Accepts:** Matrix. **One source only:** a second `add_source` replaces the first (override `accepts`, and in `draw()` use only the last source).
- **Shows:** `ImPlot::PlotHeatmap` of the latest matrix (`Series::last`, a JSON array of rows), with a colormap scale beside it. Options: `auto range` (bool, default on), else `min`/`max` inputs; colormap choice limited to Viridis/Plasma/Hot.
- **Computation to test:** `bool matrix_values(const nlohmann::json &m, std::vector<double> &out, int &rows, int &cols)` (row-major, rejects ragged/empty input, returns false) and `std::pair<double,double> value_range(const std::vector<double>&)`.
- **Note:** `Series::last` is the latest value received, so the heatmap does not follow the timeline when paused. That's accepted for M2 (matrices keep no history); document it in a `shortcut:` comment.

## T-2.5 — Cumulative boxplot view
- **Accepts:** Number.
- **Shows:** one box per source (x = source index, labelled with the keypath's last segment): median, quartiles, whiskers at 1.5 IQR, outliers as points. **Cumulative:** statistics over all samples since the view was created or last reset, **not** limited to the window or the store's capacity. Keep per-source running state inside the view: append every sample with `t` greater than the last one consumed. Options: a `Reset` menu item.
- **Drawing:** ImPlot has no boxplot: use `ImPlot::PlotBars`/`PlotErrorBars`/`PlotScatter` combinations or `ImPlot::GetPlotDrawList()` with `PlotToPixels`. Keep it simple and readable.
- **Computation to test:** `struct BoxStats { double q1, median, q3, lo, hi; std::vector<double> outliers; }; BoxStats box_stats(std::vector<double> values)` (linear-interpolated quantiles). Test against hand-computed values for a small set with an outlier, a single value, and an empty input (returns NaNs, no crash).
- **Memory:** values are kept as is (doubles). Add a `shortcut:` comment about unbounded growth (about 8 MB per million samples) and when to switch to a streaming quantile estimator.

## T-2.6 — Time-series cursor
- **Modifies:** `src/ui/views/time_series_view.{hpp,cpp}` (instead of new files; no registry change).
- **Adds:** an optional vertical cursor (`ImPlot::DragLineX`), toggled by option `cursor` (default off). When enabled it's placed at the centre of the visible range. With option `snap` (default on) it snaps to the nearest sample time of the first numeric source. Under the plot (or in a tag), show each numeric source's value at the cursor (linear interpolation; snapped: the exact sample) and a `Copy` button that copies `t<TAB>key=value…` to the clipboard (`ImGui::SetClipboardText`).
- **Computation to test:** `double value_at(const data::Series &, double t)` (linear interpolation, clamped to the ends, NaN for empty) and `double snap_time(const data::Series &, double t)` (nearest sample time).

## T-0.4 — README (writing task)
- **Goal:** replace `README.md` (create it) with a concise user and developer README for the current state (M1 + M2 groundwork).
- **Sections:** what it is (1 paragraph, Rerun-inspired dashboard for MADS); requirements (installed MADS v3 found via `mads -p`, CMake ≥ 3.28, a C++20 compiler; Linux X11/GL dev packages as in `.github/workflows/ci.yml`); build (`cmake -S . -B build -G Ninja && cmake --build build`, first build compiles HDF5, about 3 minutes); run (`./build/mads-dashboard [options] [topics...]`, options are the standard MADS agent ones: get them from `./build/mads-dashboard --help` and list the important ones); local test setup (`mads.ini`, `director.toml`, `mads director` or the three manual commands; the `signals` and `testsource` publishers; what `testsource` publishes, from `tools/testsource/src/testsource.cpp`); using the GUI (Sources tree, right-click → New view, the view types that exist in `src/ui/view.cpp`'s registry, timeline pause with Space, File → Connect); tests (`./build/tests/dash_tests`, `MADS_DASH_LIVE=1` for live tests); project docs (PLAN.md, TASKS.md); license placeholder `TBD`.
- **Facts only:** every command and option must come from the repo or from running `--help`. Don't describe features that don't exist yet; at most one line pointing to PLAN.md for the roadmap.
- **May modify:** `README.md` only.
- **Acceptance:** every command in it was run successfully or copied from the repo; under 150 lines.

---

# M3 — Timeline & data

| ID | Title | Owner | Status | Branch |
|----|-------|-------|--------|--------|
| T-3.0 | Groundwork: `View::options()/set_options()`, `Store::first_time()` | coordinator | done (6e33b59) | master |
| T-3.1 | Timeline scrubbing (review mode) + UI integration of T-3.2/T-3.3 | coordinator | done | master |
| T-3.2 | HDF5 export (library function + tests) | sonnet | done | — |
| T-3.3 | Layout save/load: view options for all views + layout (de)serialization (library + tests) | sonnet | done | — |
| T-3.4 | Record button / Open bag | — | dropped (user: use `mads record`/`mads play` + HDF5 export) | — |

Shared rules: as for M2 ("Rules shared by T-2.1 … T-2.6": build command, single plain commands, tests in `tests/test_*.cpp`, clang-format, no new dependencies, no edits outside the card's list). **No UI integration** in these cards: the coordinator wires menus and dialogs into `app_window.cpp` afterwards.

## T-3.2 — HDF5 export
- **Read first:** `src/data/store.hpp` (`Series` per `Kind`), `tests/test_version.cpp` (HighFive usage), PLAN §4.5.
- **Files:** new `src/data/export_hdf5.{hpp,cpp}` (add `src/data/export_hdf5.cpp` to `mads_dash_core` in `CMakeLists.txt`, and link `dash_hdf5` to `mads_dash_core` PRIVATE: these two CMake edits are allowed); new `tests/test_export_hdf5.cpp`.
- **Contract:** `namespace mads_dash::data { struct ExportStats { std::size_t datasets = 0, samples = 0; }; ExportStats export_hdf5(const Store &store, const std::filesystem::path &file, double t0, double t1, const std::vector<std::string> &keypaths = {}); }`. An empty `keypaths` means all. Samples with `t0 <= t <= t1` only. Throws `std::runtime_error` with a readable message on I/O failure (wrap HighFive exceptions). Overwrites `file`.
- **Layout in the file:** each keypath becomes a group path: `/testsource/accel[0]` becomes group `/testsource/accel[0]`. HDF5 accepts `[`, `]`; `/` separates groups. In each keypath group:
  - Number, Bool: datasets `t` (double, s since midnight UTC) and `value` (double; bools 0/1); attribute `kind` = `"number"` or `"bool"`.
  - String: `t` and `value` (variable-length strings) from the change events; `kind` = `"string"`.
  - Array: `t` (n) and `value` (n × len, double) from the snapshots, using the length of the first snapshot in range and skipping snapshots of a different length; `kind` = `"array"`.
  - Matrix: `value` (rows × cols) from `Series::last` only, with attribute `t` = `last_t`; `kind` = `"matrix"` (no history exists).
  - Keypaths with no samples in range are skipped (no empty groups). Root attributes: `t0`, `t1`, `created_by` = `"mads-dashboard"`.
- **Tests:** round-trip with HighFive for each kind (read back and compare values and shapes); time filtering (inclusive bounds); keypath filter; skipping empty keypaths; an unwritable path throws `std::runtime_error`.
- **Size:** about 150 lines plus tests.

## T-3.3 — Layout save/load
- **Read first:** `src/ui/view.hpp` (`options()`/`set_options()`, `ViewType` registry), every view in `src/ui/views/`, `src/ui/app_window.hpp` (what the window owns).
- **Files:** each `src/ui/views/*_view.{hpp,cpp}` (only to add `options()`/`set_options()` overrides for the settings in its `draw_options()`: TimeSeries `cursor`,`snap`; Readout `big`; XY `history`,`fade`,`markers`; Histogram `bins`,`density`; Spectrum `log_y`; Boxplot: none (cumulative state is not saved); Heatmap `auto_range`,`min`,`max`,`colormap` saved by **name**); new `src/ui/layout.{hpp,cpp}`; new `tests/test_layout.cpp`. (`src/ui/*.cpp` is globbed: no CMake edit needed.)
- **Contract (`src/ui/layout.hpp`):**
  ```cpp
  namespace mads_dash::ui {
  struct Layout {
    std::vector<std::unique_ptr<View>> views;
    double window = 10.0;   // Timeline::window
    std::string imgui_ini;  // ImGui::SaveIniSettingsToMemory() text, opaque
    int next_view_id = 1;
  };
  nlohmann::json layout_to_json(const std::vector<std::unique_ptr<View>> &views, double window, const std::string &imgui_ini);
  /// Unknown view types and malformed entries are skipped with a message in `warnings`; never throws on bad content.
  Layout layout_from_json(const nlohmann::json &j, std::vector<std::string> &warnings);
  void save_layout_file(const std::filesystem::path &, const nlohmann::json &);  // throws std::runtime_error
  nlohmann::json load_layout_file(const std::filesystem::path &);                // throws std::runtime_error (missing file, bad JSON)
  std::filesystem::path session_file();  // per-user config dir: $XDG_CONFIG_HOME or ~/.config on Linux, ~/Library/Application Support on macOS, %APPDATA% on Windows; + "/mads-dashboard/session.json"
  }
  ```
  JSON format: `{"format": 1, "window": 10.0, "imgui_ini": "...", "views": [{"type": "XY", "id": 3, "sources": [...], "options": {...}}]}`. View `id`s are kept (ImGui window titles `###view<id>` keep their docking in `imgui_ini`); `next_view_id` = max id + 1. `type` is the `ViewType::name`.
- **Tests:** a round trip of every registered type with non-default options (create via the registry, set options, serialise, deserialise, compare `options()` and `sources()`); an unknown type and a malformed entry produce warnings and are skipped; a bad option type is ignored; `next_view_id`; a file save/load round trip in a temp dir; `session_file()` ends with `mads-dashboard/session.json`.
- **Size:** about 200 lines plus tests.

---

# M4 — 3D

| ID | Title | Owner | Status | Branch |
|----|-------|-------|--------|--------|
| T-4.1 | XYZ view: 3D trace (one [x,y,z,...] array or three numbers), per-point fading, pose axes from [x,y,z,qw,qx,qy,qz] | coordinator | done | master |


# Things to fix before M5

- **Timeseries**: a timeseries panel has always the same height, even when it's the only panel in the window.
- **Time window dial**: it seems only to affect the x axis of time series, right? if so, why have it in a common area? why have it at all, since we get the same result by zooming the x axis? I'd prefer to use it as a common setting, representing:
  - for time series, the x axis length in play mode and with autoscale engaged
  - for traces, same effect of the current History setting (which is now per plot)
  - for histograms and boxplots, the time window over which to compute the histogram (the current implementation is a bit weird: it computes the histogram over the last N samples, which is not a time window)
  - for FFT, the time window over which to compute the spectrum 
  - in any case, 0 means unlimited, and the dial should be logarithmic (1, 2, 5, 10, 20, 50, 100, 200, 500, 1000 s).
  - I also prefer to have beside it a numeric field for setting the actual buffer size in seconds, and have it affect all views (dropping old messages, 0 means unlimited).
- **autoscale Y**: it continuously changes with data, which is annoying in RT plots. I'd rather expand Y to +-1/2 OOM of the current range, eg if data range is [0,10], Y = [-5,15], and sticks to that until the data goes outside it. 
- **autoscale engage**: it could be easier. My proposal is that a double click performs autoscale on all axes, and activates it. Dragging either axis disables it. 
- **boxplots**:
  * they seem not to update with data. If I clean the data, they keep the same size. 
  * autoscale does not work, everything is always scaled to [0,1]
- **Memory**: since we are accumulating data, we need a memory indicator in the top bar. The buffer length dial allows to keep memory at bay.
- **Time scale**: on time series, It would be nice to allow to select either the `timecode` field, or any other numeric field or even any field in the form `$date: {"..."}`. Right clicking on a suitable field shall have "use as timecode" in the context menu. The timecode field is then used for the x axis, and the other fields are plotted against it. This allows to plot data that is not sampled at a fixed rate, or that has a different time reference.
- **trace plots**: 
  - multiple series seem not to work: adding another series to the same trace plot does not show it.
  - Options for a trace plot shall include a per-series section, where on can select the array index to use for X and Y (and Z)
- **The `xy` field**: in the synthetic data flow, the `xy` field is a pair of arrays, and cannot be plotted in any way. Proposal: when a field contains a pair of equal size arrays, enable right click on that firls (`xy`  in the example) and offer to create a XY plot, where the first array is used for X and the second for Y. The XY plot shall be a new view type, with options for point size, color, and line style.

## Rough edges: plan (coordinator, 2026-11-21)

Decisions (user): time base **per topic** in absolute UTC; sticky autoscale on **all live plots**; boxplot **window-based** (0 = all buffered), no cumulative mode.

| ID | Item | Owner | Status |
|----|------|-------|--------|
| R-1 | Store: absolute UTC time (timecode + receive date, fixes the midnight wrap), per-topic time field ("Use as timecode": numeric or `$date`), buffer retention in seconds (0 = unlimited), memory estimate | coordinator | done |
| R-2 | Timeline: common window dial 1-2-5…1000 s + all (0), buffer field, both saved in layouts; memory in the top bar | coordinator | done |
| R-3 | `AutoScale` helper: sticky range (data ± half range, kept until data leaves it), double-click engages, left-drag/wheel/box-zoom releases; port Time series (incl. height bug), Histogram, Spectrum; window semantics for all of them | coordinator | done |
| R-4 | Boxplot: window-based statistics, sticky Y autoscale, drop cumulative state and Reset | sonnet | done |
| R-5 | XY and XYZ traces: multiple series, per-series X/Y(/Z) = (keypath, array index) in Options, common window instead of `history`, AutoScale | sonnet | assigned |
| R-6 | New "XY curve" view: latest pair of equal-length arrays (e.g. `xy.x`, `xy.y`), options point size, colour, line style; right-click on the parent node (`xy`) creates it | sonnet (view) + coordinator (tree menu) | assigned |

### Shared rules for R-4 … R-6
As for M2 ("Rules shared by T-2.1 … T-2.6": build command reusing the dependency sources, single plain shell commands, file tools for edits, tests in `tests/test_*.cpp`, clang-format, no new dependencies, no edits outside the card's list, the headless view test must pass). New since M2, read them first:
- `src/ui/auto_scale.{hpp,cpp}`: `AutoScale` (per-plot state, `begin_frame()` before `BeginPlot`, `set_hovered()` inside the plot) and `StickyRange`. **Reference implementations: `src/ui/views/time_series_view.cpp` and `src/ui/views/histogram_view.cpp`.** Pattern: compute the data first, update the sticky ranges when `on()`, then `BeginPlot`, `SetupAxes` **without** `ImPlotAxisFlags_AutoFit`, and `SetupAxisLimits(..., ImPlotCond_Always)` only when `on()`.
- `Timeline` (`src/ui/view.hpp`): the common window is `[tl.begin(), tl.now]`; `window == 0` means all buffered data. Views no longer look at `tl.paused` for scaling.
- Times are UTC epoch seconds (double).

## R-4 — Boxplot: window-based
- **Files:** `src/ui/views/boxplot_view.{hpp,cpp}`, `tests/test_boxplot.cpp`.
- **Change:** statistics per source over its samples in `[tl.begin(), tl.now]` (recomputed each frame; keep `box_stats()`). Remove the running state, the `Reset` item, the `shortcut:` about unbounded growth (no longer true), and `draw_options()` if empty. Sticky Y through `AutoScale` over [min(whisker lo, outliers), max(whisker hi, outliers)]; X stays fixed at [-0.5, n-0.5]. Hover for `set_hovered()` as in the reference views. A source with no samples in the window draws nothing (so **File → Clear data** empties the plot).
- **Tests:** keep the `box_stats` cases; add one for a window-values helper if you add one.

## R-5 — XY and XYZ traces: multiple series
- **Files:** `src/ui/views/xy_view.{hpp,cpp}`, `src/ui/views/xyz_view.{hpp,cpp}`, a new `tests/test_traces.cpp`; in `tests/test_layout.cpp` only the `"XY"` and `"XYZ"` entries of `non_default()`; in `tests/test_views.cpp` only the two test cases that are specifically about XY and XYZ (the generic all-views test stays as is).
- **Model (both views):** a list of series, each with one axis source per dimension (X, Y and, for XYZ, Z): `struct AxisSource { std::string key; int index = -1; }` (`index` is the array element for Array keypaths, ignored (-1) for Number keypaths; empty `key` = unset). Samples of a series are the times in `[tl.begin(), tl.now]` where **every** axis has a value with that exact time (Array: a snapshot with that time, element `index` if it exists; Number: a sample with that time). Base the time list on the X axis source.
- **Sync with `_sources`** (`View` is unchanged, so do it at the start of `draw()`): a source not yet used by any series is assigned by this rule: an Array (with at least 2 elements for XY, 3 for XYZ, judged from its latest snapshot) becomes a new series `(key,0),(key,1)[,(key,2)]`; a Number fills the next unset axis of the last series, or else starts a new series with it as X. Series axes whose key was removed from `_sources` become unset; a series with no set axes is dropped. Only complete series are drawn.
- **Options:** per series, a section with, per axis, a combo of the view's `_sources` plus an index input (shown for Array keys only), and a "Remove series" item; plus `fade older` and `markers` as today. Remove `history` (the common window replaces it). `options()`/`set_options()` save and restore the series (keys and indices), `fade`, `markers` (and XYZ `pose_axes`, `equal`); unknown keys, including an old `history`, are ignored.
- **Drawing:** one colour per series (`GetColormapColor(i)`), per-point fading as XYZ does today (use `ImPlotSpec::LineColors` in 2D too, if ImPlot v1.1 supports it for `PlotLine`; check `implot.h`, else keep chunked fading). XYZ pose axes: for the series whose X source is a 7-element array (latest snapshot), as today. The "two equal-length arrays as one curve" case of the old XY view is **removed**: it becomes the new XY curve view (R-6).
- **AutoScale:** XY sticky on X and Y; XYZ sticky on X, Y, Z, with `begin_frame(true)` (right-drag rotates in ImPlot3D and must not release the autoscale). ImPlot3D has no `IsPlotHovered()`: use `ImGui::IsWindowHovered()` of the view window, read after `EndPlot()`. With `equal` scale, apply one common range (the largest span, centred per axis).
- **Tests:** pure functions for the sample matching (`trace_points(...)` or similar, taking the `Store`, the series and the window) covering: one array, three numbers with mismatched times, an incomplete series, removed sources; and the source-to-series assignment rule.

## R-6 — XY curve view
- **Files:** new `src/ui/views/curve_view.{hpp,cpp}`, new `tests/test_curve.cpp`, the registry line plus include in `src/ui/view.cpp` (after `"XYZ"`), and one `"XY curve"` entry in `non_default()` of `tests/test_layout.cpp`.
- **View:** type `"XY curve"`, accepts Array. The first two sources are X and Y; others are ignored. It draws the latest pair of snapshots with **equal time** at or before `tl.now` (so it follows scrubbing) as one curve. Options: `style` (line / markers / line+markers), `marker size` (1 to 10 px), `color` (`ImGui::ColorEdit3`), saved by `options()`/`set_options()` (colour as `[r, g, b]`). AutoScale sticky on both axes over the drawn arrays.
- **Helper for the source tree** (declared in the header): `bool curve_pair(const data::Store &store, const std::string &node, std::string &x_key, std::string &y_key)`. It returns true when `node` (an intermediate tree node such as `/testsource/xy`) has exactly two direct Array children with equal latest length, setting the keys in sorted order (`x` before `y`). The coordinator wires it into the tree's right-click menu.
- **Tests:** `curve_pair` (the `xy` case, a node with one array, three arrays, unequal lengths, nested children not counted); snapshot pairing at equal time (latest pair, `now` in the past, mismatched times).
