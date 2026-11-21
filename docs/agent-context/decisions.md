# Requirements and decisions

## What the product is for

The project replaces the use of `mads-net/rerun_plugin` (a MADS sink feeding Rerun): the Rerun data model maps poorly onto hierarchical MADS messages. The dashboard is a dedicated MADS sink with a Rerun-like UI.

- **Users and context:** lab operators, researchers, students, plant engineers; lab bench, shop floor, classroom, remote monitoring.
- **Goals:** real-time interactive plots of MADS streams configured from the GUI; time series, XY/trace (2D/3D), histograms, spectra, box plots, heatmaps, readouts; pause, review and export; portable on macOS, Windows, Linux; easy to merge into the main MADS repository eventually.
- **Non-goals:** printing/reporting; online processing (filters, ML, anomaly detection). Derived data (formulas between fields) might come later.
- **Success criteria:** 20 channels at 100 Hz with < 100 ms latency on an Apple M1 laptop; start-up < 2 s on 2022 hardware. Both met (see status.md).
- **Traffic to design for:** up to ~10 agents, 10–100 Hz typical and 1 kHz peak per topic, messages < 1 kB typical and up to ~64 kB (matrices), ≤ 10k msg/s and ≤ 10 MB/s total.
- **Reliability:** never block on the network; under overload drop the oldest data, keep the newest, and show it in the UI.
- **Constraints:** C++ (or Rust), CMake, no Qt, no Boost, licence compatible with Apache-2.0/MIT, reuse the MADS client libraries. Licence of the project itself: still TBD.
- **UI reference:** Rerun (tree on the left, charts on the right, toolbar on top, timeline at the bottom), one common timeline.

## Decisions (newest last)

| Decision | Why |
|----------|-----|
| **C++20 + Dear ImGui (docking) + ImPlot + ImPlot3D + GLFW/OpenGL 3** (Rust/egui rejected). | Native MadsCore and `bag.hpp` access; same stack as the MADS GUIs (`mads-director`, `mads-chat`), so merging into MADS is easy; ImPlot/ImPlot3D cover the chart types (egui has no 3D plotting). |
| MADS is a **required installed dependency** (or the parent build), never vendored. | The dashboard is pointless without MADS, and must match its ABI. |
| "Portable" = everything except `libMadsCore` statically linked. | An installed MADS is a prerequisite anyway. |
| Keypaths use the `rerun_plugin` slash syntax (`/topic/a/b`, `[i]`). | Same paths in both tools. |
| **No Record / Open bag, no replay player** in the dashboard. | `mads record` / `mads play [--loop]` exist; a replay into the broker looks like live data. A dashboard bag reader would need a public frame decoder that MadsCore does not expose. |
| **Export = HDF5 only** (JSONL and Parquet dropped). | Parquet (Arrow C++) is too heavy to link statically on 3 OSes; HDF5 is a plain C library and fits hierarchical keypaths. Note: `mads bag export --format jsonl` does **not** decode payloads (it writes base64 wire parts). |
| XY scatter / trace / 2D trajectory merged into one **XY** view; 3D into **XYZ**; pairs of arrays got their own **XY curve** view. | Fewer view types; the array-pair case behaves differently (latest curve, not a trace). |
| 3D model as cursor in XYZ: **backlog**, not done. | Plan said "possibly"; pose axes cover the need for now. |
| **One common window dial** (1-2-5 steps to 1000 s, plus "all" = 0) drives every view (time-series span, trace length, histogram/boxplot/spectrum input), plus a **buffer** length in seconds (0 = unlimited). Per-view `history` removed. | User request: a time window, not "last N samples"; one control for all views. |
| **Sticky autoscale** on all live plots (grow to data ± half the range, then stay); double-click engages, drag/zoom/scroll releases. | User request: continuously changing axes are annoying in real-time plots. |
| **Boxplot is window-based** (0 = all buffered), no cumulative mode or Reset. | User decision; also fixed boxplots not updating or clearing. |
| **Time base per topic, in absolute UTC**, choosable per topic ("Use as time of <topic>": a number or a `$date` field). | User decision. UTC also fixes the midnight wrap. |
| MADS `timecode` is **local** time of day. | Verified in MADS (`timecode()` uses `localtime`). Treating it as UTC put timecode- and timestamp-timed topics an hour apart (CET). |
| **No prebuilt release packages.** Instead the dashboard is **FetchContent-consumable by the MADS build**, installing only `bin/mads-dashboard`. | A binary must match the ABI of the user's MADS build, which can't be ensured for downloads. |
| `tests/consumer` is **not** in CI. | User decision (an extra full build per run). |
| portable-file-dialogs pinned to commit `c12ea8c9a727f5320a2b4570aee863bbede2a204`, not release 0.1.0. | 0.1.0 passes the default file name to macOS as `default location`: save dialogs fail silently (AppleScript error -1700). |
| Font and style copied from MADS `gui_style.cpp` (monospace system font at 15 px, MADS colours). | Same look as `mads-chat`/`mads-director`; MADS does not install that code. |
| GUI strings are ASCII (`-`, `...`). | ImGui's default font has no `—`/`…` glyphs (they render as `?`); kept even with the system font, as a fallback. |

## Development process (as used so far)

The user asked for a coordinator/worker setup: Opus (the main session) owns the architecture, interfaces, reviews and the hard parts; **Sonnet** subagents implement well-specified coding cards; **Haiku** only does writing (docs). Every worker's output was reviewed, built and tested before merging. See conventions.md, "Working with subagents", for what worked and what didn't.
