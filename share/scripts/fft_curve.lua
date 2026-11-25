-- Amplitude spectrum of accel[1] over the last 128 samples, refreshed every
-- 16 samples. Show ~fft_curve/spectrum in an XY curve view.
topics = {"testsource"}

local N, EVERY = 128, 16
local ts, vs, n = {}, {}, 0

function process(_, m, t)
  if not m.accel then return nil end
  ts[#ts + 1], vs[#vs + 1] = t, m.accel[1]
  if #ts > N then table.remove(ts, 1); table.remove(vs, 1) end
  n = n + 1
  if #ts < N or n % EVERY ~= 0 then return nil end
  local s = dash.fft(ts, vs)
  return {spectrum = {x = s.f, y = s.amp}}
end
