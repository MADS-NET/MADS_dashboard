# MADS Dashboard — Project Plan

> Fill in every `TODO` and answer the guiding questions under each section; delete what doesn't apply.
> Options in `[ ]` are suggestions: tick one (`[x]`) or write your own.
> Leave a section as `TBD` if you want me to propose something.

---

## 1. Overview

- **Project name:** MADS Dashboard
- **One-line description:** Rerun-inspired tool for visualizing and analyzing real-time data streams from MADS agents.
- **Problem it solves / motivation:** Currently, we are using a MADS agent (`mads-net/rerun_plugin`), which is a Rerun plugin, to visualize data streams from MADS agents. However, this approach has limitations in terms of flexibility and user experience, for the Rerun data model is not easily mapped to hierarchical MADS data streams. The goal of this project is to create a dedicated dashboard application that can connect to MADS agents, visualize their data streams in real-time, and provide a more intuitive and user-friendly interface for researchers and engineers.
- **Target users:** lab operators, researchers, students, plant engineers
- **Usage context:** lab bench, shop floor, classroom, remote monitoring

## 2. Goals and non-goals

### Goals (must achieve)
1. Realtime, interactive plotting and visualization of [MADS data streams](https://github.com/pbosetti/mads), configurable via a user-friendly GUI.
2. Flexible and adaptable to different types of data and visualization needs, including time series, scatter plots, line plots, histograms, trace plots (2D and 3D), box plots, and more.
3. support for pausing, replaying and reviewing a captured data stream, including the ability to export data for offline analysis.
4. portable on macOS, Windows, and Linux, with minimal installation requirements and dependencies.
5. Easy to integrate, eventually, in the main MADS repository.

### Non-goals (explicitly out of scope)
- printing or reporting.
- online data filtering or processing (e.g. machine learning, anomaly detection, etc.).

### Success criteria
- plots 20 channels at 100 Hz with < 100 ms latency on a Apple M1 laptop.
- launches in < 2 seconds on 2022 average hardware.


---

## 3. Data source: network protocol

### 3.1 Transport
- **Protocol / middleware:** 
  - [X] links to the MaasdCore library, which implements the MADS network protocol, based on ZeroMQ (ZMQ) and MessagePack (MsgPack).
  - [X] behaves as a MADS sink agent, connects to a MADS broker, and receives data streams from MADS agents according to a list of subscriptions.
- **Connection endpoint(s):** the borker settings address `tcp://broker.local:9092`, or the address provided via UDP by the broker discovery service on a selected *room*.
- **Broker / discovery:** settings fetched from broker
- **Authentication / encryption:** none or CURVE, via the MaasdCore library
- **Reference implementation or docs:** 
  - [X] MADS: <https://github.com/pbosetti/mads>
  - [X] MADS docs: <https://github.com/mads-net/mads-net.github.io> source of the official documentation site (look at the qmd documents).
  - [X] Rerun plugin: <https://github.com/mads-net/rerun_plugin> reference implementation of a MADS sink agent, which connects to a MADS broker and receives data streams from MADS agents according to a list of subscriptions, feeding a [Rerun](https://rerun.io) instance for visualization.

### 3.2 Message format
- **Framing:** transparently provided by MadsCore library
- **Topics / channels:** exact, or using wildcards MQTT-type, or initial matching as in ZeroMQ
- **Payload encoding:** transparently provided by MadsCore library, exposed to the agent as a JSON object (`nlohmann::json` in C++)
- **Schema:** free JSON hierarchical structure, with the following common fields on the top level:

```json
{
  hostname: "agent1",
  ID: "sensor1",
  timestamp: "2024-06-01T12:34:56.789Z",
  timecode: 123456.78
}
```

- **Timestamp field:** ISO string
- **Timecode field:** floating-point seconds since last midnight UTC
- **Nested / array fields:** Extract as keypaths. These use the same slash syntax as `rerun_plugin`: `/<topic>/<path>/<field>`, e.g. `/signals/signals/temperature`, with array elements as `/<topic>/acceleration[0]`.
  - **Scalar fields:** use as time series (numbers) or markers (strings)
  - **Array fields:** use as time series (one per element) or XY plots (e.g. 2D/3D trajectory)
  - **Matrix fields:** use as heatmaps
- **Non-numeric fields:** use as event markers

### 3.3 Traffic characteristics
- **Number of sources/agents:** up to ~10
- **Message rate per topic:** typically 10-100 Hz, peak 1 kHz
- **Message size:** typically < 1 kB, max ~64 kB (matrices)
- **Expected total throughput:** design target ≤ 10k msg/s, ≤ 10 MB/s
- **Behaviour on disconnect / sender restart:** keep existing data, mark the source as stale in the tree, resume appending when it comes back (proposal)

### 3.4 Test data
- **Is there a simulator / replay tool?** Yes, we can use a local running MADS broker and a two mock publishers that run the `mads-net/signals_plugin` signal generators
- **Sample recordings available?** not needed since we have the signal generator plugin.
- **Local test setup (in the repo root):**
  - `mads.ini`: broker on `tcp://localhost:9090/9091/9092`, plus a `[signals]` source publishing at 10 Hz on topic `signals`, nested under `signals` (`temperature`: sine; `pressure`: square + noise). It also has a `[rerunner]` section that serves as a reference for subscriptions and keypaths.
  - `director.toml`: starts `mads broker -d`, then `mads source signals.plugin`, via `mads director`.
  - The dashboard reads its own `[dashboard]` section from the same `mads.ini` (agent name `dashboard`; `sub_topic` defaults to `[""]`, meaning everything).
  - To do: add a second signals instance (different name and topic, higher rate) to cover the 2-publisher and multi-rate cases, and a high-rate profile (1 kHz) for performance tests.
- **Dedicated test source plugin (`testsource.plugin`, in this repo under `tools/testsource/`):** `signals` cannot publish arrays or matrices, so we build our own MADS source plugin. It publishes on a configurable topic and rate:
  - a numeric array (e.g. `accel: [x, y, z]`);
  - a 3D pose as position, or as position + quaternion (`pose: [x, y, z, qw, qx, qy, qz]`) moving along a known trajectory (e.g. a Lissajous curve);
  - an XY pair of arrays (`xy: {x: [...], y: [...]}`, e.g. a sampled waveform);
  - a matrix (`map: [[...], ...]`, e.g. a moving 2D Gaussian, size configurable up to ~64 kB per message);
  - string and bool fields that change on events (for markers and readouts).
  - Stress tests use `period` (1 ms = 1 kHz) and the matrix size (90×90 ≈ 64 kB); there's no separate stress profile.

  It's built as an optional CMake target (`MADS_DASH_BUILD_TESTSOURCE`, ON by default) against the installed MADS plugin headers, and added to `director.toml`.

---

## 4. Functional requirements

### 4.1 Connection management
- select the room to join, or enter broker settings address `tcp://broker:9092`
- also support room or broker settings via command line arguments
- file selector for picking CURVE keys (if needed)

### 4.2 Data selection
- tree browser of incoming topics and fields, with right-click to adda a chart
- sources of alredy created charts can be changed by selecting from the three browser, similarly to how Rerun works

### 4.3 Charts
- **Chart types needed:**
  - [X] Time series (strip chart) · [X] XY / scatter of X and Y arrays, updated in real time, with optional fading history (alpha channel on previous lines/points) · [X] XY(Z) trace plots, with options for variable length memory · [X] Numeric readout, also for non-numeric fields (strings, booleans) 
  - [X] Histogram · [X] Spectrum / FFT · [X] 2D/3D trajectory · [X] Heatmap · [X] other: cumulative boxplot (resettable via GUI)
- **Multiple series per chart?** YES
- **Multiple charts / layout:** grid, tabs, dockable panels
- **Time window:** fixed rolling window or user-adjustable
- **Interaction:** pause/resume, zoom, pan, cursors, hover values, autoscale, fixed ranges, snap and copy cursor value
- **Refresh rate target:** min 30fps, optionally 60fps, with a user-adjustable refresh rate
- **Decimation / downsampling acceptable?** YES, with user-adjustable decimation factor

### 4.4 Derived data (optional)
- Not immediately, but it might be needed in future releases

### 4.5 Recording and export
- **Recording:** raw stream in the common MADS **bag** format (`bag.hpp`: `BagWriter` / `BagReader` from the installed MADS), so recordings work with other MADS tools.
- **Replay:** open a bag file and replay it into the store through the common timeline.
- **Export:** **JSONL** and **HDF5** (both always built in), either for the whole session or for a selected time range and set of keypaths. Parquet is dropped. HDF5 is linked statically and used only for export, never for live recording; live recording uses bag files.
  - HDF5 layout: one group per topic, keypaths mapped to nested groups, and per-keypath datasets (`timecode` plus a value column, or N columns for arrays).

### 4.6 Configuration and persistence
- save/load dashboard layouts, last session restore

### 4.7 Other features
- Possibly load a 3D model to be shown as cursor in 3D trace views; when used, the coordinate is accepted as a three element array (position) or as a position + quaternion array in the JSON message

---

## 5. Non-functional requirements

- **Target platforms:**
  - [X] macOS (Intel / Apple Silicon) · [X] Windows · [X] Linux (x86_64 / ARM, e.g. Raspberry Pi)
- **"Portable" means:** 
  - [X] single self-contained executable · [X] no installation / no admin rights · [X] runs from USB stick
  - **Clarification:** an installed MADS is a prerequisite (§6). "Self-contained" therefore means that all *other* dependencies (GUI, plotting, Parquet, ...) are linked statically, and the only runtime dependency is the MADS installation's `libMadsCore`.
- **Performance:** TODO (max channels, points on screen, latency, CPU/RAM budget)
- **Minimum hardware:** TODO
- **Offline operation required?** NO
- **Reliability:** must never block on network; behaviour under overload: drop oldest data, keep newest data, and show a warning in the UI
- **Accessibility / UI constraints:** TODO (dark mode, high-DPI, touchscreen, min window size)

---

## 6. Technology preferences and constraints

- **Language:** 
  - [X] C++ · [X] Rust
- **GUI framework:** 
  - [X] Dear ImGui (+ ImPlot) · [X] egui
- **Charting library:** TODO
- **Build system / packaging:** CMake
- **Libraries that MUST be used:** TODO
- **Libraries / tech to AVOID:** QT, boost
- **License constraints:** Apache 2.0 or MIT
- **Integration with existing code:** reuse existing MADS client libraries / headers
- **MadsCore linkage:** use the **installed** MADS (`find_package(Mads REQUIRED)` → target `Mads::Mads`, shared `libMadsCore`). The dashboard is not meant to exist without MADS, so MADS is not fetched or vendored.
- **Stack choice:** **decided: C++20 + Dear ImGui (docking) + ImPlot + ImPlot3D + GLFW/OpenGL 3.** Rust/egui was considered and rejected: no native MadsCore or bag access, no 3D plotting, harder to merge into MADS.

---

## 7. Architecture (proposal — to be confirmed)

> Stack (decided): **C++20 + Dear ImGui (docking branch) + ImPlot + ImPlot3D, GLFW + OpenGL 3**, built with CMake.
> GUI dependencies come via `FetchContent`; MADS comes from the installed `find_package(Mads)`. This is the same stack as
> the existing MADS GUIs (`src/gui/`, `gui_style`), so it can be merged into the main repo later (Goal 5).

- **High-level components:**
  - `net::Receiver`: a `Mads::Agent` sink running on its own thread. It calls `receive()` with a timeout, never touches the UI, and pushes `(topic, timecode, json)` messages into a bounded queue.
  - `data::Flattener`: turns JSON into keypaths (`/topic/field/sub[2]`, see §3.2) with typed values: number, string/bool (event), numeric vector, numeric matrix. It also registers new keypaths in the source tree.
  - `data::Store`: one ring buffer per keypath, with capacity limits. It drops the oldest data, keeps an overflow counter and shows a UI warning. Timecode is the index; columns are typed.
  - `data::Recorder` / `data::Player`: record the raw stream to disk (reusing the MADS `bag` format if it fits) and play it back into the same `Store`.
  - `ui::Timeline`: live, paused and review modes; time cursor; scrubbing; replay speed.
  - `ui::SourceTree`: tree browser (Rerun-style). Right-click → "add to new/existing view".
  - `ui::View`: abstract interface (`draw()`, `sources()`, `to_json()/from_json()`). There is one subclass per chart type, and new chart types are added this way.
  - `ui::Layout`: ImGui docking, plus JSON files for layouts and sessions, plus last-session restore.
- **Threading model:** a network thread, the UI/render thread and an optional recorder thread. The network thread feeds an SPSC queue. At the start of each frame the UI thread drains it into the `Store`, so the `Store` has a single owner and needs no locks.
- **Data buffering strategy:** per-keypath ring buffers with a configurable capacity in seconds or samples. Decimation (min/max per pixel bucket) happens at draw time, not at storage time.
- **Plugin/extension mechanism needed?** Not at runtime. View types and parsers sit behind internal interfaces, so new ones are just new classes.

```
 MADS broker ──ZMQ──▶ [net::Receiver thread] ──SPSC queue──▶ [UI thread]
                              │                                 │ drain → Flattener → Store
                              └──▶ [Recorder thread] ──▶ file    │ Timeline / SourceTree / Views
                                                                 ▼
 file ──▶ [data::Player] ───────────────────────────────▶   ImGui + ImPlot render @ 30-60 fps
```

---

## 8. UI sketch

- **Timeline:** **one common timeline** for all views (Rerun-style). Pause, scrub and replay act on every view together.
- **Main window layout:** use Rerun as a reference, with a tree browser on the left, a chart area on the right, and a toolbar on top, and the timeline on the bottom.

- **Reference apps you like (and why):** Rerun, for its UI design, speed and robustness.

---

## 9. Testing and quality

- **Testing expectations:** TODO (unit tests, integration tests with mock publisher, UI tests)
- **CI:** TODO (GitHub Actions? which OS matrix?)
- **Code style / linting:** TODO
- **Documentation deliverables:** README, user manual, developer docs

---

## 10. Development workflow (multi-agent)

Development is run by one **coordinator** model that delegates well-specified work to **worker** models.

### 10.1 Roles

| Role | Model | Responsibilities |
|------|-------|------------------|
| **Coordinator** | Claude Opus (main session) | Owns `PLAN.md`, `TASKS.md`, the architecture and every public interface (headers, data contracts, CMake structure). Splits milestones into task cards and dispatches them. Verifies and integrates all worker output. Implements the complex parts itself (see 10.3). Reports to the user and raises decisions. |
| **Coding workers** | Claude Sonnet (subagents, `model: sonnet`) | Do one well-specified coding task card at a time, against interfaces the coordinator has already fixed. They write the implementation **and** its tests, and report what they did and any doubts. |
| **Writing workers** | Claude Haiku (subagents, `model: haiku`) | Non-coding tasks only: documentation drafts, user manual pages, README sections, changelog entries, doc comments on finished code, summaries of long build/test logs. They never change code that gets compiled. |
| **User** | — | Decides on product questions, approves milestones, and authorizes commits/merges as agreed. |

### 10.2 What goes to workers

**Sonnet (coding):**
- Implementing a class or function behind a header the coordinator wrote: e.g. the JSON flattener, ring buffer, JSONL/HDF5 exporters, layout (de)serialization, CLI argument parsing.
- Individual `ui::View` subclasses once the `View` interface and one reference view exist (histogram, boxplot, heatmap, numeric readout, ...).
- Unit tests for existing code; test fixtures; mock data generators.
- CMake/CI scripts that follow an existing pattern; packaging scripts per OS. These count as code because their mistakes only show up on other platforms.
- Repetitive refactors and renames.

**Haiku (non-coding):**
- Documentation drafts, written from finished and merged code plus the coordinator's outline.
- Doc comments added to finished headers, with no change to the code itself.
- Changelog and release notes taken from merged task cards.
- Condensing long CI or test logs into a short failure summary for the coordinator.

### 10.3 What the coordinator keeps (complex / cross-cutting)
- Architecture, module boundaries, public headers, threading model and the queue/Store ownership.
- MadsCore integration: agent lifecycle, settings and discovery, CURVE.
- The timeline and live/paused/replay model, and recording/replay semantics.
- The 3D pipeline (ImPlot3D traces, 3D-model cursor with quaternion pose).
- Performance work (decimation, profiling against the success criteria in §2), cross-platform build and portable packaging.
- Anything that fails verification twice (see 10.5), and any task that needs touching more than one module.

### 10.4 Task cards
Every delegated task is a card in `TASKS.md` written by the coordinator, with these fields:

```
ID:           T-<milestone>.<n>
Title:        short imperative
Goal:         what must exist / work when done
Context:      files to read first (headers, PLAN.md sections)
Contract:     exact interface to implement (signatures, data formats) — not to be changed
May modify:   explicit list of files/directories
Must not:     touch public headers, top-level CMake, PLAN.md, other modules
Acceptance:   observable criteria + the exact build/test command that must pass
Size:         target ≤ ~300 changed lines; larger work is split into more cards
```

Status flow: `todo → assigned → review → done` (or `→ rework → review`, or `→ escalated`).

### 10.5 Execution and verification protocol
1. **Contract first:** the coordinator commits the headers/stubs and the failing or empty tests before dispatching.
2. **Isolation:** each worker runs in its own git worktree/branch (`task/T-x.y`). Independent cards run in parallel. Cards that touch the same files run one after another.
3. **Worker report:** files changed, build/test output, assumptions made, open doubts.
4. **Coordinator verification**, all of which must pass:
   - a clean build on the local platform, plus the full test suite;
   - a diff review covering scope (only allowed files), correctness, thread-safety, error handling, style (clang-format, LLVM), and no new dependencies unless approved;
   - acceptance criteria checked by actually running things (and the app against the mock publishers when the task affects the UI or networking).
5. **Outcome:** merge into `master`, or send it back to the same worker with concrete feedback. After **2 failed rework rounds** the coordinator takes the task over.
6. **Integration check** after each merged batch: full build plus a smoke run with the broker and two `signals_plugin` publishers.

### 10.6 Conventions
- C++20, clang-format LLVM style, `#pragma once`, namespaces `mads_dash::{net,data,ui,app}`.
- Dependencies come only via CMake `FetchContent` with pinned tags. New dependencies need the coordinator's approval; licenses must be compatible with Apache-2.0/MIT.
- No Qt, no Boost (§6).
- Commit messages: `T-x.y: <title>`.
- **Git policy:** the coordinator may commit on the default branch (`master`) and create `task/T-x.y` branches without asking. Commits are always made **in the user's name** (the repo's git identity, with no Claude author or co-author trailer). Only verified work is committed, and nothing is pushed and no remote is added without asking.

---

## 11. Milestones (proposal)

| # | Milestone | Scope | Target date |
|---|-----------|-------|-------------|
| 0 | Skeleton | Repo, CMake + FetchContent (MadsCore, ImGui docking, ImPlot, GLFW), empty docked window, CI building on 3 OSes, static HDF5 build check, `testsource.plugin` (arrays, pose, matrix, events), local test setup documented | TODO |
| 1 | MVP | Connect via broker address or room, receiver thread, flattener, store, source tree, time-series view, pause/resume, CLI args | TODO |
| 2 | Views | XY with fading, scatter, numeric/string readout, histogram, FFT, heatmap, cumulative boxplot; cursors and hover; decimation | TODO |
| 3 | Timeline & data | Bag recording, bag replay, review scrubbing, export to JSONL/HDF5, layout save/load and last-session restore | TODO |
| 4 | 3D | 2D/3D traces with memory length, 3D model cursor (position / position + quaternion) | TODO |
| 5 | Release | Performance validation vs §2, CURVE, portable packages per OS, user manual | TODO |

---

## 12. Open questions and risks

- **Risk:** ImPlot3D is younger than ImPlot. Its features and performance for long 3D traces need checking early (spike in M0/M1).
- **Risk:** fully self-contained executables on Linux (glibc, OpenGL drivers). An AppImage is probably the realistic target.
- **Risk:** worker output quality on UI code. Mitigated by having the coordinator write a reference implementation per category first.
- **Risk:** the HDF5 static build adds build time and binary size. Check it in M0 on all 3 OSes, and pick the wrapper there (C API vs HighFive v3).
- Open questions: see the coordinator's latest questions to the user (to be folded in here once answered).

## 13. Glossary

| Term | Meaning |
|------|---------|
| MADS | Multi Agent Distributed System, a framework for building distributed systems with multiple agents, transparently using ZeroMQ  |
| Agent | A process that participates in the MADS system, sending and receiving data |
| Broker | A process that acts as an intermediary between agents, routing messages and managing connections, also providing shared settings |
| Topic | A category or subject to which data is related, used for organizing and filtering data streams |
| Plugin | Some MADS agents can be implemented as plugins, runtime loaded by a general purpose agent |
| Source | A MADS agent that produces data streams, e.g. a sensor or a simulator |
| Filter | A MADS agent that receives data streams, processes them, and produces new data streams |
| Sink | A MADS agent that receives data streams and consumes them, e.g. a visualization tool or a data logger |
| Keypath | Address of a single value inside a message, e.g. `/signals/signals/temperature` or `/imu/acceleration[0]` |
| Coordinator / Worker | Development roles defined in §10 (Opus coordinates; Sonnet subagents do coding task cards; Haiku subagents do writing tasks) |
| Task card | A self-contained unit of delegated work, tracked in `TASKS.md` (§10.4) |

