-- Roll/pitch/yaw in degrees to a quaternion, the inverse of euler.lua (same
-- convention: yaw about z, then pitch about y, then roll about x). Returns
-- q = {qw, qx, qy, qz} and pose = {x, y, z, qw, qx, qy, qz}: add ~quaternion/pose
-- to an XYZ view to see the rotation as body axes.
topics = {"testsource"}

-- Where the angles come from: replace with your own fields. testsource has no
-- angles, so this demo makes them from accel (roll +-90, pitch +-45, yaw +-180).
local function angles(m)
  if not (m.accel and m.position) then return nil end
  return m.accel[1] * 90, m.accel[2] * 45, m.accel[3] * 180, m.position
end

local rad = math.pi / 180

function process(_, m)
  local roll, pitch, yaw, pos = angles(m)
  if not roll then return nil end
  local cr, sr = math.cos(roll * rad / 2), math.sin(roll * rad / 2)
  local cp, sp = math.cos(pitch * rad / 2), math.sin(pitch * rad / 2)
  local cy, sy = math.cos(yaw * rad / 2), math.sin(yaw * rad / 2)
  local w = cr * cp * cy + sr * sp * sy
  local x = sr * cp * cy - cr * sp * sy
  local y = cr * sp * cy + sr * cp * sy
  local z = cr * cp * sy - sr * sp * cy
  return {
    q = {w, x, y, z},
    pose = {pos[1], pos[2], pos[3], w, x, y, z},
    roll = roll, pitch = pitch, yaw = yaw,
  }
end
