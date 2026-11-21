# Status (as of 2026-11-21)

All planned milestones are done: skeleton and CI (M0), receiver, store, tree, time series (M1), the view types (M2), timeline scrubbing, HDF5 export, layouts and session (M3), 3D (M4), performance check, CURVE, MADS build integration and user manual (M5), plus a round of rough-edge fixes requested by the user after trying it.

## Features

- Connect by settings URI or room (CLI or File → Connect), optional CURVE (keys folder, key names, missing-file check); auto-subscribe to all topics without a `[dashboard]` section; top bar with state, msg/s, memory, drops, fps.
- Sources tree with keypaths, kinds, latest values; right-click: new view per accepting type, add to view, use as time of topic; node menu: back to MADS timecode, New XY curve view.
- Views: Time series (cursor with snap/values/copy), Readout, XY, XYZ (pose axes, equal scale), XY curve, Histogram, Spectrum (FFT), Boxplot, Heatmap.
- One timeline: window dial, buffer, pause/resume (Space), scrub; sticky autoscale with double-click/drag.
- Layouts (save/load), session restore, HDF5 export (whole session or window), `--layout`, `--stats`.

## Measured performance (Apple A18 Pro, 8 GB, isolated broker, `--stats`)

| Load | Result |
|------|--------|
| 30 channels, 2 sources at ~105 Hz (211 msg/s) | 57 fps, lag (source `timestamp` → frame) median 10 ms, p99 < 25 ms, max 26 ms |
| 1 source at ~930 Hz, 54 kB messages (90×90 matrix, 256-point arrays) | 58 fps, lag median 10.5 ms, max 23 ms, 0 dropped; memory grows ~4 MB/s (array history), bounded by the buffer |
| Start-up to first frame | 906 ms cold, 350 ms warm |

CI time per run: macOS ~2 min, Linux ~5 min, Windows ~11 min (mostly HDF5 with MSBuild; Ninja on Windows would need an MSVC environment action, and the usual one, `ilammy/msvc-dev-cmd`, still runs on deprecated Node 20).

## Known limitations

- **One shared timeline:** a topic timed by a non-clock field (e.g. simulation seconds) is out of view while clock-timed topics run, because `now` is the newest time of all topics (its data are kept, and visible when panning or when it's alone). A proper fix is a timeline per topic.
- Assumes the sender and the dashboard share a time zone for `timecode` (use the `timestamp` `$date` field as time otherwise).
- Heatmap shows only the latest matrix (matrices keep no history), so it doesn't follow scrubbing.
- Time-series decimation keeps every n-th point: a short spike can vanish when zoomed out (min/max per pixel would keep it).
- Spectrum and boxplot recompute everything every frame; the time-series sticky range scans the window each frame. Fine at the tested loads; cache per series if long windows at kHz rates lag (`shortcut:` comments mark the spots).
- Trace views auto-assign a newly added source to a series; the alternative (assign axes only by hand in Options) was offered, not requested.
- A string readout's age is the time since the value last *changed*.
- `--version` prints the MADS version (AgentApp's option), not the dashboard's.
- Start-up prints "Using settings timeout of 3000 ms" twice (from MADS).
- Not seen on screen by the developer: most GUI details were verified by the user; the CURVE part of the Connect dialog was only tested through the equivalent CLI options.
- Not yet tried: building inside the real MADS build (only the `tests/consumer` stand-in). The MADS-side snippet is in the README ("Building inside MADS").

## Backlog / ideas (not requested yet)

- Per-topic timelines (see above).
- 3D model as cursor in XYZ views (from the original plan, "possibly").
- Derived data (formulas between fields), mentioned for future releases.
- Marking a source as stale in the tree when its agent stops (original proposal, not implemented).
- Licence for the project (TBD).
- In MADS itself: `--room` (and other implicit-value options) silently turn `--opt value` into a topic filter; a public frame decoder in MadsCore would enable bag reading and decoded `mads bag export`.
- Leftover worktrees of finished workers under `.claude/worktrees/` (locked by the agent system; all their branches are merged) can be removed once unlocked.
