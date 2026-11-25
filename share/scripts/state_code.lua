-- State string to a number (idle 0, run 1, hold 2) and the seconds spent in
-- the current state. Use a Time series, or a Histogram of the code.
topics = {"testsource"}

local codes = {idle = 0, run = 1, hold = 2}
local last, since

function process(_, m, t)
  if not m.state then return nil end
  if m.state ~= last then last, since = m.state, t end
  return {code = codes[m.state] or -1, seconds = t - since}
end
