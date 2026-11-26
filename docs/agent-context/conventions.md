# Conventions, procedures and pitfalls

## Build and test

```bash
cmake -S . -B build -G Ninja          # first build compiles HDF5: a few minutes
cmake --build build
./build/tests/dash_tests              # or: ctest --test-dir build --output-on-failure
```

- MADS is found through `mads -p` (installed under `~/usr/local` on the owner's Mac). `-DMads_DIR=<prefix>/lib/cmake/Mads` overrides it.
- A second build tree (worktree, experiment) can reuse the downloaded sources and skip the downloads: `-DFETCHCONTENT_SOURCE_DIR_GLFW=<main>/build/_deps/glfw-src` and `-DFETCHCONTENT_SOURCE_DIR_MADS_DASH_<NAME>=<main>/build/_deps/<name>-src` for `IMGUI`, `IMPLOT`, `IMPLOT3D`, `HDF5`, `HIGHFIVE`, `PFD`, `DOCTEST` (check the actual `*-src` folder names in `build/_deps`).
- Live tests are opt-in: `MADS_DASH_LIVE=1` (needs a broker and `testsource` on the default ports), `MADS_DASH_RECONNECT=uri|room` (start the broker and `testsource` about 5 s after launching the test).
- Check the MADS-consumer mode after any CMake change: `cmake -S tests/consumer -B /tmp/consumer -G Ninja && cmake --build /tmp/consumer --target mads-dashboard` (not in CI, by decision).

## Code conventions

- C++20, `clang-format` LLVM style (`.clang-format`; on the Mac the binary is `/Library/Developer/CommandLineTools/usr/bin/clang-format`), `#pragma once`, namespaces `mads_dash::…`.
- Comments only for the *why*; `shortcut: <limit>, <when to upgrade>` comments mark known limits (grep for them).
- New dependencies only via FetchContent at a pinned tag, named `mads_dash_*`, licence compatible with Apache-2.0/MIT, statically linked; never force generic cache variables (see architecture.md, "Build").
- Keep the smallest complete change; the owner prefers minimal code (they use the "ponytail" skill: no speculative options or abstractions).
- Commits: in the **user's name** (the repo's git identity), **no** co-author or "Generated with" trailers. Commit on `master` and create task branches freely; **never push** (the user pushes) and never add remotes. Only commit verified work. When the user says "don't commit", don't.

## Testing patterns

- Pure functions in headers, tested with doctest (`tests/test_<name>.cpp`, globbed).
- **Headless view test** (`tests/test_views.cpp`): ImGui/ImPlot/ImPlot3D run without a window or GPU; every registered view type is drawn for every keypath it accepts, live and paused. ImGui asserts throw in tests (`dash_im_assert_throws`), so API misuse fails the suite. A deliberate misuse test prints `Calling PopID() too many times!`: that line is expected.
- **Time-zone independence:** test stores call `set_fixed_utc_offset(0)`; never rely on the machine's time zone.
- The layout test requires an entry per registered view type in `non_default()` (forces new views to support layouts).
- Keep test code portable to MSVC: e.g. `1.0 / 0.0` is a compile error there (use `std::numeric_limits<double>::infinity()`); Windows cannot delete open files (close HDF5 files before `remove`).

## MADS facts and quirks (verified)

- **Options with an implicit value** (`--room`, `--keys_dir`, `--key_client`, `--key_broker`) must be passed as **one** argument `--opt=value`: with `--opt value` the value becomes a positional topic filter. This broke "Connect by room" (subscribed to a topic named `mads`). `connect_args()` handles it.
- `AgentApp::parse_options()` exits the process on invalid options.
- `timecode` = seconds since **local** midnight, quantised to 1/`timecode_fps`; `timestamp` = `{"$date": "<local ISO 8601 with offset>"}` in MADS v3.
- `mads source` waits the full `period` **after** each tick: `period = 10` ≈ 83 Hz, `period = 8` ≈ 105 Hz, `period = 1` ≈ 930 Hz (Apple A18 Pro).
- MADS installs its signal handlers only in `Agent::loop()`; the dashboard installs its own (SIGINT/SIGTERM → `mads_stop_process()`).
- Transport `auto` uses ipc sockets in `/tmp/mads-<uid>/` when the broker is local; killed brokers leave stale socket files (harmless, a new broker replaces them).
- `mads echo --jsonl --count N <topic>` (with `-s` or `--room=`) is the quickest way to inspect live messages; its `size` field gives the message size.
- CURVE: `mads --keypair=NAME` writes `NAME.key`/`NAME.pub` in the current directory; broker `--crypto --keys_dir=DIR` needs `broker.key` and the clients' `.pub`; agents need `--crypto` plus `client.key`, `client.pub`, `broker.pub`.
- MADS headers: `~/usr/local/include` (`mads/agent.hpp`, `mads/agent_app.hpp`, `mads/agent_c.h`, `mads/bag.hpp`, `mads/mads.hpp`); plugin docs: `~/usr/local/share/skills/mads-plugin/`; MADS source: `github.com/pbosetti/mads` (`CONTEXT.md` is a good primer).

## Running MADS processes (safety)

- **The user often runs their own `mads director`, broker, `signals` and `testsource`.** Before starting anything, check with `pgrep -fl "mads|broker|testsource|signals.plugin"` and `lsof -nP -iTCP:9090-9092 -sTCP:LISTEN`.
- **Never stop processes by name** (`pkill -f "broker -d"` once killed the user's director-managed broker and testsource). Start your own processes in the background, keep their PIDs (`cmd & PID=$!`) and `kill` only those.
- For benchmarks and experiments, use an **isolated broker** on other ports (9190–9192) with its own `mads.ini` in a scratch folder, and start agents with `-s tcp://localhost:9192`; the user's setup then sees nothing.
- Running the GUI opens a window on the user's screen: say so. Running the app also rewrites the user's session file on exit: back it up (`~/Library/Application Support/mads-dashboard/session.json`) and restore it after scripted runs.
- In zsh, don't put several options in one variable (`A="--crypto --keys_dir=X"` passes one argument).

## Working with subagents (lessons)

- Give each worker a written card: goal, files it may change, exact contract, acceptance commands. Workers do best on one view or one library function with its tests; keep cross-cutting work (store, timeline, CMake, MADS integration) in the main session.
- **Create worktrees yourself** from current `master` (`git worktree add .claude/worktrees/<name> -b task/<id>`) and tell the worker the path. Automatic worktrees were sometimes created from an **old commit**, and were removed when a worker stopped without changes (breaking resumes).
- Tell workers: single plain shell commands (the worktree guard refuses complex or `source`-containing command lines), file tools instead of heredocs, and **stop and report** on any refusal, never work around it.
- Review every diff; build and run the full suite after merging; resolve registry conflicts in `src/ui/view.cpp` yourself.
- Workers that finish with a leftover background process keep re-sending their report: stop them once merged.
- Editor diagnostics about files in `.claude/worktrees/` are noise (no compile database there).
