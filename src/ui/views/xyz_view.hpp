#pragma once

#include "ui/views/xy_view.hpp"

#include <array>

namespace mads_dash::ui {

/// Rotates `v` by the unit quaternion `q` = (w, x, y, z); `q` is normalised
/// first, and a zero quaternion leaves `v` unchanged.
std::array<double, 3> rotate(std::array<double, 4> q, std::array<double, 3> v);

/// 3D trace with several series, each (x(t), y(t), z(t)) over
/// the timeline's window (see XYView and sync_series for how sources become
/// series). For a series whose X source is a 7-element array
/// [x, y, z, qw, qx, qy, qz], the latest pose is drawn as red/green/blue body
/// axes (option `pose axes`).
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
  std::vector<TraceSeries> _series;
  std::set<std::string> _seen;   ///< sources already given to a series
  std::set<std::string> _arrays; ///< sources that are Arrays, as of draw()
  AutoScale _scale;
  bool _fade = true;
  bool _markers = false;
  bool _pose_axes = true;
  /// Same units per pixel on x, y and z: without it each axis is fitted to
  /// its own range, which distorts lengths and angles (and the pose axes).
  bool _equal = true;
};

} // namespace mads_dash::ui
