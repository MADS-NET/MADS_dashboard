# MADS Dashboard user manual

The MADS Dashboard (`mads-dashboard`) shows live data from a MADS broker in a desktop window. It subscribes to the topics you choose, receives the JSON messages that agents publish, and plots them in views that you arrange on screen. It is read-only: it never publishes to the broker and it does not change the agents.

This manual assumes you know MADS: brokers, agents, topics and `mads.ini`. It covers how to use the dashboard, not how it is built (see `README.md` for that).

## 1. The window

The window has four areas:

- **Menu bar** (top): the File menu, the connection state, the message rate, the memory used, the frame rate and the MADS version. See section 2.3.
- **Sources** (left): a tree of every keypath received so far, with the latest value of each field. Right-click a field to create a view from it. See section 3.
- **Views** (centre): the charts. Each view is its own window. Drag views to rearrange them. New views appear here as tabs.
- **Timeline** (bottom): pause and resume, the time window, the buffer size and a scrub slider. See section 5.
- **Scripts** (a tab next to Sources): Lua scripts that compute derived data from the received messages. See section 8.

## 2. Start and connect

### 2.1 From the command line

```bash
mads-dashboard [options] [topic ...]
```

Each `topic` is a subscription filter. MQTT-style wildcards are allowed. Without topics, the settings decide. If the settings have no `[dashboard]` section, the dashboard subscribes to all topics.

Run `./build/mads-dashboard --help` for the list on your build. The options are:

| Option | Default | Meaning |
|--------|---------|---------|
| `-s, --settings ARG` | none | Settings file path or URI, for example `tcp://localhost:9092` (the broker's settings address). |
| `-r, --room[=NAME]` | `mads` when given without a value | Join by room discovery. Write `--room=NAME` as one word. In `--room NAME`, NAME is read as a topic filter, so nothing is received. |
| `--settings-timeout MS` | `3000` when omitted (the dashboard adds it) | Timeout for reading settings from the broker. `0` means no timeout. |
| `--transport auto\|tcp\|ipc` | MADS default when omitted | Transport to the broker. `auto` uses ipc when the broker runs on this host. |
| `--crypto` | off | Enable CURVE encryption for broker communication. |
| `--keys_dir DIR` | `<MADS prefix>/etc` | Folder where the CURVE keys are stored. |
| `--key_client NAME` | `client` | Client key file name, without `.key`. |
| `--key_broker NAME` | `broker` | Broker key file name, without `.key`. |
| `--auth_verbose` | off | Verbose authentication messages. |
| `-S, --save-settings FILE` | none | Save the settings to an INI file. |
| `-n, --name ARG` | none | Agent name. |
| `-i, --agent-id ARG` | none | Agent ID to be added to JSON frames. |
| `-v, --version` | | Print the MADS version and exit. |
| `-h, --help` | | Print usage and exit. |

Dashboard-only options:

| Option | Default | Meaning |
|--------|---------|---------|
| `--stats` | off | Print performance statistics to stderr every second (section 2.4). Implies `--verbose`. |
| `--verbose` | off | Run in the foreground, with stdout/stderr, so Ctrl+C quits. Without it the command returns at once and the GUI runs detached from the terminal, its output discarded. |
| `--layout FILE` | last session | Open this layout file at start instead of the last session (section 9). `--layout=FILE` works too. |

Examples:

```bash
./build/mads-dashboard -s tcp://localhost:9092 testsource signals
./build/mads-dashboard --room=mads testsource
./build/mads-dashboard --stats -s tcp://localhost:9092 signals
./build/mads-dashboard --layout ~/layouts/lab.json -s tcp://localhost:9092
```

### 2.2 File → Connect

Use **File → Connect...** to change the broker while the dashboard runs. Connecting replaces the current connection.

| Field | Default | Meaning |
|-------|---------|---------|
| Broker settings URI / Room (discovery) | URI | How to find the broker. |
| URI | `tcp://localhost:9092` | The broker's settings address. Used in URI mode. |
| Room | `mads` | The room to join by discovery. Used in room mode. |
| Topics | empty | Space-separated topic filters, MQTT wildcards allowed. Empty means the settings decide. |
| Encrypted (CURVE) | off | Tick it to connect to a broker started with `mads broker --crypto`. |
| Keys folder | `<MADS prefix>/etc` | Folder with the keys. *Browse...* picks a folder. |
| Client key | `client` | Client key name. |
| Broker key | `broker` | Broker key name. |

With CURVE on, the folder needs `<client>.key`, `<client>.pub` and `<broker>.pub`. The broker needs `<client>.pub`. The dialog lists any missing file in red. Keys are created with `mads --keypair=NAME`.

The File menu also has:

- **Clear data**: empties the received data. Time settings (section 4) are kept.
- **Save layout...** and **Load layout...**: see section 9.
- **Export HDF5: whole session...** and **Export HDF5: timeline window...**: see section 9.3.
- **Quit**: exits.

### 2.3 The top bar

The menu bar shows, from left to right:

- The connection state: `connecting`, `connected` (green), `failed` (red) or `disconnected`. When connected, the endpoint is shown next to it. When failed, the error text is shown.
- The message rate (messages per second).
- The memory used by the buffered data, in MB.
- `dropped N (overload)` in red when messages were lost. The dashboard holds up to 100 000 messages between two frames. If it falls behind, it drops the oldest messages and counts them.
- The frame rate and the MADS version.
- A short notice after a file action, such as a saved layout or an error.

### 2.4 Statistics (`--stats`)

With `--stats`, the dashboard prints to stderr once a second:

- frame rate and messages per second;
- end-to-end lag in ms (p50, p99 and max): from each message's `timestamp` to the frame that shows it. Only messages with a `timestamp` in `$date` form are counted;
- dropped messages, keypaths, data in MB and views.

At the first frame it also prints the start-up time, from process start to the first frame.

## 3. The Sources tree

### 3.1 Keypaths

Every value has a keypath: `/` followed by the topic, then one `/` for each field name, and `[i]` for each array index.

- `/topic/a/b` is field `b` inside object `a` of the topic. Example: `/signals/signals/temperature`.
- `/topic/arr[i]` is element `i` of an array. A numeric array of up to 16 elements also gets one keypath per element. Example: `/testsource/accel[0]`.
- Other arrays (of objects, or mixed values) get `[i]` for each element, and their fields are shown in turn.
- `null`, empty arrays and empty objects are not shown.
- A top-level `timestamp` string is not shown. A `timestamp` in extended JSON, `{"$date": ...}`, is shown as a number at `/topic/timestamp/$date`.

The tree groups keypaths by their `/` parts. Hover over a field to see its keypath and its last value.

### 3.2 Kinds of data

The first value of a keypath sets its kind. A later value of another kind is ignored.

| Kind | Example | Sources tree shows | Accepted by views |
|------|---------|--------------------|-------------------|
| Number | `temperature` | the value | Time series, Readout, XY, XYZ, Histogram, Spectrum, Boxplot |
| Bool | `alarm` | `true` or `false` | Time series, Readout |
| String | `state` | the text | Time series (as event lines), Readout |
| Array | `accel`, `position`, `pose` (numeric list) | `[n]` | Readout, XY, XYZ, XY curve |
| Matrix | `map` (equal-length numeric rows) | `[RxC]` | Heatmap |

### 3.3 Right-click menus

Right-click a field to see:

- **Use as time of <topic>**: for a number, or a `$date` field, that is not already the time of its topic. See section 4.
- **New <type> view**: one entry for each view type that accepts the kind. The new view starts with this field as its source.
- **Add to <view>**: one entry for each existing view that accepts the kind.
- **No view for this kind of data**: shown when no view accepts the kind.

Right-click a topic name (the top node of its tree) to see its current time field. If it is not the MADS timecode, **Use MADS timecode as time** appears.

Right-click a node that has exactly two array fields of equal length, such as `xy` with `x` and `y`, to get **New XY curve view** (section 7.5).

The field that gives its topic's time is greyed and marked `(time scale)`. This is `timecode` by default.

## 4. Time

- Sample times are UTC. Time axes show clock time, and the timeline shows `HH:MM:SS.mmm UTC`.
- By default, the samples of a topic are timed by its MADS `timecode`. MADS timecode is seconds since local midnight. The dashboard converts it with the UTC offset of its own machine, so the sender and the dashboard must share a time zone. A timecode more than 12 hours from the time of reception is taken to belong to the previous or the next day. A message without a timecode is timed by its reception time.
- To time a topic by another field, right-click a number, or a `$date` field, and choose **Use as time of <topic>**. A `$date` field is an ISO 8601 string (`2024-06-01T12:34:56.789Z`; with `+02:00` style offsets; with no zone it is UTC) or a number of milliseconds since the epoch.
- Choosing a time field clears the received data of that topic. Data of other topics are kept. To go back, right-click the topic and choose **Use MADS timecode as time**.
- A numeric time field whose values are not epoch-like (below 10^8) is shown as plain seconds, labelled `t [s]`, instead of clock time.

## 5. The Timeline

- **Pause / Resume** (the button, or Space when no text box has keyboard focus). While paused, the views show the time at the timeline's position. The data keep arriving and are buffered. Resume returns to the newest data.
- **window**: how many seconds of data every view shows or uses. The steps are 1, 2, 5, 10, 20, 50, 100, 200, 500 and 1000 s, and `all`. The default is 10 s. Time series show this span. XY, XYZ, Histogram, Boxplot and Spectrum use the same span as their input. `all` uses everything in the buffer.
- **buffer [s]**: data older than this are dropped. Age is counted from the newest sample of each topic. The default is 600 s. `0` keeps everything; the memory is then shown in the top bar. The buffer is saved in layouts.
- The line `(live)` or `(paused)` shows the state. It is followed by the count of messages and keypaths.
- **Scrub**: while paused, the slider moves the timeline's time through the buffered data, from the oldest to the newest sample. All views follow it.

## 6. Plot interaction

- Every plot starts with **autoscale** on. An axis grows when the data leave its range. The new range is the data range plus half of it on each side. For example, data from 0 to 10 give an axis from -5 to 15. The axis then stays put until the data leave it again.
- **Double-click** a plot to turn autoscale on and fit the data again.
- **Drag** (left button), **box-zoom** or **scroll** turns autoscale off, so you can set the axes by hand. In 2D plots, a right-button drag does the same. In XYZ, a right-button drag rotates the view and keeps autoscale on.
- Dragging the time series cursor does not turn autoscale off.

## 7. Views

Each view window has two menus:

- **Sources**: lists the keypaths of the view. Choose **Remove <keypath>** to take one out.
- **Options**: the settings of the view type, listed below.

Each view is titled by its type and number, for example `Time series #3`. Views are saved in layouts (section 9).

### 7.1 Time series

- **Accepts**: Number, Bool, String.
- **Shows**: numbers and bools as lines against time (bools as 0 and 1). Strings as vertical lines, labelled with each new value. The span is the timeline window.

| Option | Default | Meaning |
|--------|---------|---------|
| `markers` | off | Draws a marker at each drawn point. When a window holds more points than the plot has pixels, only the thinned-out points that are drawn get a marker. |
| `cursor` | off | Adds a vertical cursor that you can drag. Below the plot it shows the value of each numeric source at the cursor, and a **Copy** button. Copy puts the time and the values on the clipboard, tab-separated. |
| `snap` | on | Shown when `cursor` is on. The cursor jumps to the nearest sample of the first numeric source. |

### 7.2 Readout

- **Accepts**: Number, Bool, String, Array.
- **Shows**: a table with the keypath, the value at the timeline time and the age of that value in seconds. Arrays show up to 8 elements. `-` means no value yet.

| Option | Default | Meaning |
|--------|---------|---------|
| `big values` | on | Doubles the font size of the value column. |

### 7.3 XY

- **Accepts**: Number, Array.
- **Shows**: one or more traces (series) of X against Y over the timeline window.
- **Adding sources**: an Array with at least two elements becomes one series, using its element 0 as X and element 1 as Y. A Number fills the next free axis of the last series. So two numbers make one series: the first is X and the second is Y.
- **Pairing**: a point is drawn when X and Y have values with the same time. Values from one message share a time.

| Option | Default | Meaning |
|--------|---------|---------|
| `Series N`: X and Y | set when added | The source of each axis. For an Array source, an index input selects the element (0 or more). |
| `Remove series` | | Removes the series. |
| `fade older` | on | Older points are drawn fainter. |
| `markers` | off | Draws a marker at each point. |

### 7.4 XYZ

- **Accepts**: Number, Array.
- **Shows**: the same as XY, in 3D. An Array with at least three elements becomes one series, using elements 0, 1 and 2. Numbers fill the axes in turn, as in XY. Right-drag rotates the view.

| Option | Default | Meaning |
|--------|---------|---------|
| `Series N`: X, Y and Z | set when added | As in XY. |
| `Remove series` | | As in XY. |
| `fade older` | on | As in XY. |
| `markers` | off | As in XY. |
| `pose axes` | on | If the X source of a series is a 7-element pose array `[x, y, z, qw, qx, qy, qz]`, its latest pose is drawn as three body axes: red for x, green for y and blue for z. Each axis is 10% of the largest extent of the trace. |
| `equal scale` | on | The three axes have the same scale, so lengths and angles are true. |

### 7.5 XY curve

- **Accepts**: Array only.
- **Shows**: one curve made of two arrays of equal length, for example `/testsource/xy/x` against `/testsource/xy/y`. It shows the newest pair of snapshots that have the same time, at or before the timeline time. It does not keep a history.
- **Creating it**: right-click the parent node (`xy`) and choose **New XY curve view**. The node must have exactly two array fields, of equal length. The first in name order is X, the second is Y.

| Option | Default | Meaning |
|--------|---------|---------|
| `style` | line + markers | `line`, `markers` or `line + markers`. |
| `marker size` | 4 | Size of the markers, from 1 to 10. |
| `color` | blue (RGB 0.12, 0.47, 0.71) | Colour of the curve. |

### 7.6 Histogram

- **Accepts**: Number.
- **Shows**: the values in the timeline window. All sources share the same bins, so they can be compared. Each source is drawn at 50% fill.

| Option | Default | Meaning |
|--------|---------|---------|
| `bins` | 30 | Number of bins, from 5 to 200. |
| `density` | off | Off: counts per bin. On: density. |

### 7.7 Spectrum

- **Accepts**: Number.
- **Shows**: the amplitude spectrum of each source over the timeline window. At least four samples are needed.
- **Method**: the samples are resampled to a uniform grid, at the mean sample rate of the window. The mean is removed, a Hann window is applied, and the data are zero-padded to a power of two. A radix-2 FFT gives the spectrum. The frequency axis runs from 0 Hz to half the sample rate, and the axis unit is Hz.

| Option | Default | Meaning |
|--------|---------|---------|
| `log Y` | off | Logarithmic amplitude axis. Zero amplitudes are not drawn. |

### 7.8 Boxplot

- **Accepts**: Number.
- **Shows**: one box per source, from the values in the timeline window. The box runs from the first to the third quartile, with a line at the median. The whiskers reach the most extreme values within 1.5 times the interquartile range. Values beyond the whiskers are drawn as circles. Each box is labelled with the last part of its keypath.
- **Options**: none.

### 7.9 Heatmap

- **Accepts**: Matrix.
- **Shows**: the latest matrix of the last source added. Other sources are not drawn. It always shows the newest matrix, so it does not follow the timeline when paused. A colour bar is drawn at the right.

| Option | Default | Meaning |
|--------|---------|---------|
| `auto range` | on | The colour scale runs from the smallest to the largest value of the matrix. |
| `min`, `max` | 0 and 1 | The colour scale when `auto range` is off. |
| `colormap` | Viridis | `Viridis`, `Plasma` or `Hot`. |

## 8. Scripts (derived data)

A script is a small Lua 5.4 file that computes new data from the received messages. Examples are a filtered signal, the norm of a vector, Euler angles from a quaternion, or a spectrum. Its result is published *inside the dashboard* as the topic `~<name>`, where `<name>` is the file name without `.lua`. The derived topic appears in the Sources tree like any other topic, so every view can plot it, and layouts and HDF5 export include it. Nothing is sent to the broker.

### 8.1 Script folders

The **Scripts** panel lists folders of scripts. Each folder is one line that expands to show the `.lua` files it holds; subfolders are not searched. A long path is shortened to its last two folders: hover over it to see it in full.

- The list always starts with `<MADS prefix>/share/mads-dashboard/scripts` (`<MADS prefix>` is the output of `mads -p`). Installing the dashboard puts the example scripts there. This folder cannot be removed.
- **Add folder...** adds a folder to the list.
- Right-click a folder for **Open folder** (in the file manager) and **Remove folder** (from the list; no file is deleted; not available for the default folder).
- The list is kept between runs, in `mads-dashboard/script_folders.json` in the per-user config folder (next to `session.json`, see section 9.1).

Edit the scripts in any text editor: the dashboard reloads a file about a second after it changes, and also picks up new and deleted files. The file name is the output topic, so two folders cannot both hold a script of the same name: the second one shows an error and cannot be enabled, until the first is renamed or its folder removed.

Under each folder, the panel lists every script with a checkbox. Scripts start **off**: tick one to run it. Next to each script the panel shows *off*, *waiting for data*, the number of calls and the mean time per call, or *error* followed by the message. The enabled scripts are saved in the session and in layout files, under `scripts`.

### 8.2 Writing a script

```lua
-- low-pass filter and norm of testsource's accel
topics = { "testsource" }       -- input topics; { "" } means every topic

local a, lp = 0.1, { 0, 0, 0 }
function process(topic, msg, t)  -- msg: the message as a Lua table; t: its time
  for i = 1, 3 do lp[i] = lp[i] + a * (msg.accel[i] - lp[i]) end
  local n = math.sqrt(msg.accel[1]^2 + msg.accel[2]^2 + msg.accel[3]^2)
  return { accel_lp = lp, norm = n }
end
```

- `topics` lists the input topics, with the same rules as a MADS `sub_topic`: MQTT wildcards (`+`, `#`) and prefix matching. Derived `~` topics are never script inputs.
- `process(topic, msg, t)` is called once for every message of those topics. `msg` is the JSON payload as a Lua table: objects have string keys, arrays are 1-based (`msg.accel[1]` is the keypath `accel[0]`). `t` is the sample time in seconds, from the topic's time base (section 4).
- What `process` returns becomes one message of `~<name>`, timed at `t`, so it lines up with its input on the timeline:
  - a table: its fields become keypaths, `/~smooth/norm`, `/~smooth/accel_lp`, `/~smooth/accel_lp[0]`, and so on;
  - a number, bool or string: the keypath `/~<name>/value`;
  - `nil`: nothing. Use it to decimate or to emit only now and then.
- Variables outside `process` keep their values between calls, so a script can hold filter states, counters and ring buffers.
- The kind of data each result has decides which views accept it (section 3.2):

| Return | Kind | Views |
|--------|------|-------|
| a number or a bool | Number, Bool | Time series, Readout, XY, XYZ, Histogram, Spectrum, Boxplot |
| an array of numbers | Array | XY, XYZ, Readout; with a second array of equal length under the same table, XY curve |
| `{ x = {...}, y = {...} }` | two Arrays | XY curve (for example a spectrum: frequency against amplitude) |
| an array of equal-length number arrays | Matrix | Heatmap (for example a waterfall of spectra) |
| a string | String | Time series (event lines), Readout |

- Available: the Lua `string`, `math` and `table` libraries, the base functions, `os.clock` and `os.time`. Not available: files (`io`, `dofile`), processes (`os.execute`) and modules (`require`).
- `dash.fft(t, v)`: amplitude spectrum of samples `v` taken at times `t` (two arrays of equal length). It works like the Spectrum view and returns `{ f = {...}, amp = {...} }`.

### 8.3 Errors

- A syntax error, or a script without a `process` function, shows *error* and the message (with the file name and line) in the panel. The script cannot be enabled until the file is fixed.
- An error while running `process` (for example a field missing from a message) turns the script off and shows the message. Tick it again to retry, or fix the file: saving reloads it.
- A call that runs too long (more than about a million Lua instructions, for example an endless loop) is stopped in the same way, so a script cannot freeze the dashboard.
- Scripts run in the dashboard's drawing loop. The time per call in the panel tells you what a script costs: at 1000 messages per second, 100 µs per call takes a tenth of the time.

### 8.4 The example scripts

They all read the `testsource` topic (section 10.1):

| Script | Computes | View |
|--------|----------|------|
| `smooth.lua` | low-pass filtered `accel`, the derivative of `accel[0]`, the norm | Time series |
| `euler.lua` | roll, pitch and yaw in degrees from the `pose` quaternion, and the position | Time series, XYZ |
| `quaternion.lua` | the inverse: a quaternion `q` and a 7-element `pose` from roll, pitch and yaw (for the demo, made from `accel`; replace its `angles()` function with your own fields) | XYZ (pose axes), Time series |
| `spectrogram.lua` | spectra of `accel[0]` over the last 128 samples, the last 64 of them as rows | Heatmap |
| `fft_curve.lua` | the spectrum of `accel[0]` as `{x, y}` | XY curve |
| `autocorr.lua` | the normalised autocorrelation of `accel[0]` over the last 128 samples, against the lag in seconds, as `{x, y}` | XY curve |
| `state_code.lua` | `state` as a number (idle 0, run 1, hold 2) and the seconds spent in it | Time series, Histogram |

The examples are installed in `<MADS prefix>/share/mads-dashboard/scripts`, and are also in the repository under `share/scripts/`. To change one, copy it to a folder of your own and rename it, as installing again overwrites them.

### 8.5 Heavier processing

For work that needs NumPy or SciPy, machine learning, or should also feed other agents, write a MADS **filter** agent (any language MADS supports). It subscribes to the input topics and publishes its results on a new topic, and the dashboard plots that topic like any other. Scripts are for quick, light preprocessing inside the dashboard.

## 9. Layouts, session and export

### 9.1 Session

- At start-up the dashboard restores the last session: the views and their options, the docking, the timeline window and the buffer. Data are not restored.
- On exit (closing the window, File → Quit, Ctrl+C with `--verbose`, SIGINT or SIGTERM) the session is saved to `mads-dashboard/session.json` in the per-user config folder:
  - macOS: `~/Library/Application Support`;
  - Linux: `$XDG_CONFIG_HOME` or `~/.config`;
  - Windows: `%APPDATA%`.
- `--layout FILE` opens that file instead of the last session. The session file is still written on exit.

### 9.2 Layout files

- **File → Save layout...** writes a JSON file, `layout.json` by default.
- **File → Load layout...** opens a JSON file. It replaces the current views, the timeline window and the buffer.
- A layout file has `format` (1), `window` (seconds), `buffer` (seconds), `imgui_ini` (the docking state), `scripts` (the enabled scripts, section 8) and `views`. Each view has `type`, `id`, `sources` and `options`. Views of unknown type, or with missing fields, are skipped with a message on stderr.

### 9.3 HDF5 export

**File → Export HDF5** has two items:

- **whole session**: from the first to the latest received time;
- **timeline window**: from the start of the timeline window to the current time.

The items are enabled once messages have arrived. The default file name is `mads_dashboard.h5`. Only keypaths with samples in the range are written.

The file layout is:

| Location | Content |
|----------|---------|
| `/` (root) | Attributes `t0` and `t1`: the start and end of the export, in UTC seconds. `created_by`: `mads-dashboard`. |
| `/<keypath>` | One group per keypath, named after it, for example `/signals/signals/temperature`. Attribute `kind`: `number`, `bool`, `string`, `array` or `matrix`. |
| `.../t` | Times, in seconds, one per sample: UTC seconds, or the values of the time field if you chose one (section 4). Not present for `matrix`. |
| `.../value` | The samples. Numbers and bools (0 or 1) are 1D. Strings are text. Arrays are 2D, one row per sample. A matrix is 2D, holding its latest value only. |
| `.../t` of a matrix | Attribute `t` (a scalar) on the group: the time of the latest matrix. Written only if that time is in the range. |

Here is how to read the file with Python and `h5py` (`pip install h5py`). It uses the keypaths of the sample setup in section 10:

```python
import h5py
from datetime import datetime, timezone

with h5py.File("mads_dashboard.h5", "r") as f:
    print(f.attrs["t0"], f.attrs["t1"])        # export range, UTC seconds
    temp = f["/signals/signals/temperature"]   # a number keypath
    print(temp.attrs["kind"])                  # "number"
    t = temp["t"][:]                           # times, UTC seconds
    value = temp["value"][:]
    print(datetime.fromtimestamp(t[0], tz=timezone.utc), value[0])

    def show(name, obj):                       # list every keypath
        if "kind" in obj.attrs:
            print(name, obj.attrs["kind"])
    f.visititems(show)
```

## 10. Local test setup

The repository root has a local setup for trying the dashboard.

- **`mads.ini`**:
  - `[broker]`: frontend on `tcp://*:9090`, backend on `tcp://*:9091`, settings on `tcp://*:9092`.
  - `[signals]`: a source with `period = 100` (ms, so 10 Hz), publishing on topic `signals` and nested under `signals`. Its fields are `temperature` (a sine) and `pressure` (a square wave with a little noise).
  - `[testsource]`: `period = 10` (ms) and the settings in the table below.
- **`director.toml`** starts `mads broker -d`, then `mads source signals.plugin`, then `mads source build/testsource/testsource.plugin` (the path the build produces). `signals.plugin` is not part of this repository. It must be installed where `mads` can find it.

Start the setup with `mads director` from the repository root, or with three commands in three terminals:

```bash
mads broker -d
mads source signals.plugin
mads source build/testsource/testsource.plugin
```

Then start the dashboard:

```bash
./build/mads-dashboard -s tcp://localhost:9092 testsource signals
```

### 10.1 What `testsource` publishes

`testsource` publishes one message per tick on topic `testsource`. Each message has these fields:

- `accel`: `[ax, ay, az]`, three sines at 1, 2 and 3 Hz, with a little noise.
- `position`: `[x, y, z]`, a 3D Lissajous curve.
- `pose`: `[x, y, z, qw, qx, qy, qz]`, the same position with a rotation about z.
- `xy`: `{"x": [...], "y": [...]}`, a sine that moves in time.
- `map`: a 2D Gaussian whose centre moves on a circle. It is omitted when either size is 0.
- `state`: a string that cycles through `idle`, `run` and `hold`, changing every `state_period` seconds.
- `alarm`: a bool, true when `accel[0] > 0.9`.
- `counter`: an integer tick count.

Its settings, in `[testsource]`, are all optional:

| Key | Default | Meaning |
|-----|---------|---------|
| `period` | none in the plugin; `10` in `mads.ini` | Loop period in ms. The MADS host consumes it. The host sleeps the full period after each tick, so `period = 10` gives about 83 Hz (measured on an Apple-silicon Mac), not 100 Hz. |
| `xy_points` | 64 | Samples in `xy`. |
| `matrix_rows`, `matrix_cols` | 32, 32 | Size of `map`. |
| `state_period` | 2.0 | Seconds per `state` value. A value of zero or less gives 2.0. |

## 11. Troubleshooting

| Symptom | Cause and fix |
|---------|---------------|
| Connected by room on the command line, but the Sources tree stays empty. | `--room NAME` with a space reads NAME as a topic filter, so nothing matches. Write `--room=NAME`. (The Connect dialog does this for you.) |
| Plots empty right after switching the time field. | Switching clears that topic's data: plots fill again as new messages arrive. If they stay empty, the topic's time field may not be a clock (section 4) while other topics are: the timeline follows the newest time of all topics, so pause and scrub, or drag the time axis, to find the data. |
| `missing: ...` in red in the Connect dialog. | A CURVE key file is not in the keys folder. Add the file, or change the folder or the key names. See section 2.2. |
| File dialogs do not open on Linux. | The file dialogs (layouts, export, *Browse...*) use `zenity` or `kdialog`. Install one of them. |
| `dropped N (overload)` in the top bar. | The dashboard is falling behind. Subscribe to fewer topics, or reduce the message rate or size. |
| `failed` in the top bar. | The error text is shown next to it. Check the URI, and that the broker is running. Settings are read with the settings timeout (3 s by default). |
| `Waiting for a numeric matrix...` in a Heatmap. | The last source added is not a matrix (equal-length rows of numbers). Check its kind in the Sources tree. |
| `XY: pick X and Y per series in Options`. | No series is complete. Pick the sources of each axis in the view's Options. |
| No pose axes in an XYZ view. | The X source of the series is not a 7-element array. |
| Time axis shows `t [s]`, not clock time. | The time field is plain seconds (section 4). This is expected. |
| `No view for this kind of data` in the right-click menu. | No view type accepts that kind (section 3.2). Pick a field of another kind. |
