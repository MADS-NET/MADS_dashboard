# testsource plugin for MADS

This is a Source plugin for [MADS](https://github.com/MADS-NET/MADS).

It publishes synthetic data with every shape the MADS Dashboard has to handle: numbers, arrays, a 3D pose, a pair of arrays, a matrix, a string and a bool. Use it to try the dashboard's views, and for load tests. Each message carries the fields below, plus the usual MADS fields (`timestamp`, `timecode`, `hostname`, and `agent_id` when set).

*Required MADS version: 3.0.0.*

## Published fields

`t` is the time in seconds since the plugin received its settings (steady clock).

| Field | Type | Content |
|-------|------|---------|
| `accel` | array of 3 numbers | sines at 1, 2 and 3 Hz, amplitude 1, plus gaussian noise (σ = 0.02) |
| `position` | array of 3 numbers | 3D Lissajous curve: `x = sin(0.3t)`, `y = sin(0.4t + π/2)`, `z = 0.5 sin(0.5t)` |
| `pose` | array of 7 numbers | `[x, y, z, qw, qx, qy, qz]`: the same position, and a unit quaternion for a rotation about z by `0.2t` |
| `xy` | object `{"x": [...], "y": [...]}` | `xy_points` samples: `x` uniform on [0, 2π], `y = sin(x + t)` (a travelling sine) |
| `map` | matrix (array of rows) | `matrix_rows` × `matrix_cols` 2D gaussian (σ = 0.1 of the side) whose centre moves on a circle with a 10 s period; values rounded to 4 decimals; left out when either size is 0 |
| `state` | string | cycles `idle` → `run` → `hold`, changing every `state_period` seconds |
| `alarm` | bool | true when `accel[0] > 0.9` |
| `counter` | integer | number of messages published, from 0 |

## Supported platforms

Currently, the supported platforms are:

* **Linux**
* **MacOS**
* **Windows**

## Installation

The MADS Dashboard build compiles it as part of its own build (option `MADS_DASH_BUILD_TESTSOURCE`, on by default), into `build/testsource/testsource.plugin`. Run it from there, as in the dashboard's `director.toml`:

```bash
mads source build/testsource/testsource.plugin
```

To build and install it on its own, into the MADS prefix:

Linux and MacOS:

```bash
cmake -Bbuild -DCMAKE_INSTALL_PREFIX="$(mads -p)"
cmake --build build -j4
sudo cmake --install build
```

Windows:

```powershell
cmake -Bbuild -DCMAKE_INSTALL_PREFIX="$(mads -p)"
cmake --build build --config Release
cmake --install build --config Release
```

## INI settings

The plugin supports the following settings in the INI file:

```ini
[testsource]
period = 10          # ms, loop period of the MADS host (its default is 100)
xy_points = 64       # number of samples in xy.x and xy.y (at least 1)
matrix_rows = 32     # rows of map (0 leaves map out)
matrix_cols = 32     # columns of map (0 leaves map out)
state_period = 2.0   # seconds between changes of state (must be > 0)
```

All settings are optional; if omitted, the default values above are used. `period` is read by the MADS source host, not by the plugin: without it, the host uses its own default of 100 ms.

Notes for load tests:

* The host waits `period` *after* each message, so the actual rate is a little lower: `period = 10` gives about 83 Hz on an Apple-silicon Mac, and `period = 1` about 930 Hz.
* Message size is driven by the matrix and the arrays: a 90 × 90 matrix with 256 `xy_points` gives about 54 kB per message (measured).

## Executable demo

On macOS the built `testsource.plugin` is also an executable. Running it directly, without MADS, uses the default settings and prints three consecutive messages as JSON, one per line, each prefixed with `Output:`:

```bash
build/testsource/testsource.plugin
```

---
