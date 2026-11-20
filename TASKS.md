# MADS Dashboard — Task board

Maintained by the coordinator. Protocol: PLAN.md §10.
Status: `todo → assigned → review → done` (or `rework`, `escalated`).

| ID | Title | Owner | Status | Branch |
|----|-------|-------|--------|--------|
| T-0.1 | Build skeleton: CMake, dependencies, empty docked window linked to MadsCore | coordinator | done | master |
| T-0.2 | `testsource.plugin`: arrays, pose, XY, matrix, events | sonnet | assigned | task/T-0.2 |
| T-0.3 | CI (GitHub Actions, 3 OSes, MADS installed on runners) | sonnet | blocked (needs remote + MADS install recipe for CI) | — |
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
