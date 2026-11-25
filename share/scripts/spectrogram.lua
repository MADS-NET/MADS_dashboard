-- Spectrogram of accel[1]: every 16 samples the spectrum of the last 128
-- samples becomes a new row of a 64-row matrix. Show ~spectrogram/waterfall
-- in a Heatmap.
topics = {"testsource"}

local N, EVERY, ROWS = 128, 16, 64
local ts, vs, rows, n = {}, {}, {}, 0

function process(_, m, t)
  if not m.accel then return nil end
  ts[#ts + 1], vs[#vs + 1] = t, m.accel[1]
  if #ts > N then table.remove(ts, 1); table.remove(vs, 1) end
  n = n + 1
  if #ts < N or n % EVERY ~= 0 then return nil end
  rows[#rows + 1] = dash.fft(ts, vs).amp
  if #rows > ROWS then table.remove(rows, 1) end
  return {waterfall = rows}
end
