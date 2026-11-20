#pragma once

#include "ui/view.hpp"

#include <array>

namespace mads_dash::ui {

/// Rotates `v` by the unit quaternion `q` = (w, x, y, z); `q` is normalised
/// first, and a zero quaternion leaves `v` unchanged.
std::array<double, 3> rotate(std::array<double, 4> q, std::array<double, 3> v);

/// 3D trace (PLAN §4.3, M4). Sources, by count and kind:
///  - one Array [x, y, z, ...]: trajectory of its first three elements; if it
///    has 7 elements [x, y, z, qw, qx, qy, qz], the latest pose is drawn as
///    red/green/blue body axes (option `pose axes`);
///  - three Numbers (X, Y, Z): a trace of samples with equal time.
/// Shows the last `history` samples up to the timeline's `now`, older ones
/// fading out when `fade` is on.
class XYZView : public View {
public:
  using View::View;
  const char *type_name() const override { return "XYZ"; }
  bool accepts(data::Kind k) const override {
    return k == data::Kind::Number || k == data::Kind::Array;
  }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;
  nlohmann::json options() const override;
  void set_options(const nlohmann::json &j) override;

private:
  int _history = 500;
  bool _fade = true;
  bool _markers = false;
  bool _pose_axes = true;
  /// Same units per pixel on x, y and z: without it each axis is fitted to
  /// its own range, which distorts lengths and angles (and the pose axes).
  bool _equal = true;
};

} // namespace mads_dash::ui
