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
| T-2.1 | Readout view | sonnet | assigned | task/T-2.1 |
| T-2.2 | Histogram view | sonnet | assigned | task/T-2.2 |
| T-2.3 | Spectrum (FFT) view | sonnet | done | — |
| T-2.4 | Heatmap view | sonnet | done | — |
| T-2.5 | Cumulative boxplot view | sonnet | assigned | task/T-2.5 |
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
