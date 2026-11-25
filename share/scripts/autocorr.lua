-- Autocorrelation of accel[1] over the last 128 samples, for lags 0..64
-- samples, refreshed every 16 samples. x is the lag in seconds, y the
-- normalised autocorrelation (1 at lag 0). Show ~autocorr/acf in an XY curve.
topics = {"testsource"}

local N, MAXLAG, EVERY = 128, 64, 16
local ts, vs, n = {}, {}, 0

function process(_, m, t)
  if not m.accel then return nil end
  ts[#ts + 1], vs[#vs + 1] = t, m.accel[1]
  if #ts > N then table.remove(ts, 1); table.remove(vs, 1) end
  n = n + 1
  if #ts < N or n % EVERY ~= 0 then return nil end
  local mean = 0
  for i = 1, N do mean = mean + vs[i] end
  mean = mean / N
  local d = {}
  for i = 1, N do d[i] = vs[i] - mean end
  local dt = (ts[N] - ts[1]) / (N - 1) -- mean sample period
  local lag, r, r0 = {}, {}, 0
  for i = 1, N do r0 = r0 + d[i] * d[i] end
  if r0 == 0 then return nil end -- constant signal: undefined
  for k = 0, MAXLAG do
    local s = 0
    for i = 1, N - k do s = s + d[i] * d[i + k] end
    lag[k + 1], r[k + 1] = k * dt, s / r0
  end
  return {acf = {x = lag, y = r}}
end
