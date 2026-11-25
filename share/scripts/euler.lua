-- Pose quaternion (pose[4..7] = qw, qx, qy, qz) to roll/pitch/yaw in degrees,
-- plus the position pose[1..3]. Use a Time series (roll, pitch, yaw) or an
-- XYZ view (pos).
topics = {"testsource"}

local deg = 180 / math.pi

function process(_, m)
  local p = m.pose
  if not p then return nil end
  local w, x, y, z = p[4], p[5], p[6], p[7]
  local s = math.max(-1, math.min(1, 2 * (w * y - z * x)))
  return {
    roll = math.atan(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)) * deg,
    pitch = math.asin(s) * deg,
    yaw = math.atan(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)) * deg,
    pos = {p[1], p[2], p[3]},
  }
end
