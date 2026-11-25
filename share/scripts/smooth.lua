-- Low-pass (alpha 0.1) of the three accel channels, derivative of accel[1]
-- and the accel norm. Plot ~smooth/lp[1], ~smooth/d1, ~smooth/norm in a
-- Time series.
topics = {"testsource"}

local alpha = 0.1
local lp, prev_t, prev_a

function process(_, m, t)
  local a = m.accel
  if not a then return nil end
  if not lp then
    lp = {a[1], a[2], a[3]}
  else
    for i = 1, 3 do lp[i] = lp[i] + alpha * (a[i] - lp[i]) end
  end
  local d = 0
  if prev_t and t > prev_t then d = (a[1] - prev_a) / (t - prev_t) end
  prev_t, prev_a = t, a[1]
  return {lp = lp, d1 = d, norm = math.sqrt(a[1]^2 + a[2]^2 + a[3]^2)}
end
