# Agent context: MADS Dashboard

Read this folder before changing the project. It replaces the original `PLAN.md` (requirements and plan) and `TASKS.md` (task board), and records what was learnt while building the dashboard (Nov 2026), so nobody has to rediscover it.

| File | Read it for |
|------|-------------|
| [architecture.md](architecture.md) | How the code fits together: threads, the store, time base, views, autoscale, layouts, the build. |
| [decisions.md](decisions.md) | What the product must do, what was decided and dropped, and why. Check it before "fixing" something that is deliberate. |
| [conventions.md](conventions.md) | How to build, test and change the code; the MADS quirks that cost time; process and safety rules (git, running brokers, workers). |
| [status.md](status.md) | What is done, measured performance, known limitations and the backlog. |

User-facing documentation lives elsewhere: `README.md` (build, run, overview) and `docs/manual.md` (the user manual). Keep both in step with behaviour changes.

## First steps

1. Build and run the tests (see [conventions.md](conventions.md), "Build and test"):
   ```bash
   cmake -S . -B build -G Ninja && cmake --build build && ./build/tests/dash_tests
   ```
2. Read `src/ui/view.hpp` (View interface, Timeline, registry) and `src/data/store.hpp` (Store, Series, time base): almost every change touches one of them.
3. For a GUI change, try it live with the local test setup (`mads.ini`, `director.toml`, `testsource`), **after checking that the user is not already running MADS processes** (see conventions.md, "Running MADS processes").

## The project in one paragraph

`mads-dashboard` is a Rerun-inspired desktop GUI that connects to a MADS broker as a read-only sink agent, flattens every received JSON message into keypaths (`/topic/a/b`, `/topic/arr[i]`), buffers them per keypath, and draws them in dockable views (time series, readout, XY, XYZ, XY curve, histogram, spectrum, boxplot, heatmap) that share one timeline. It is written in C++20 with Dear ImGui (docking), ImPlot, ImPlot3D and GLFW/OpenGL 3, links the MADS library (`libMadsCore`) and nothing else at runtime, and can be built standalone against an installed MADS or by the MADS build itself (FetchContent). The project owner is also the author of MADS.
