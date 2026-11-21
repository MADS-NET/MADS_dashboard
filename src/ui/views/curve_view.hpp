#pragma once

#include "ui/auto_scale.hpp"
#include "ui/view.hpp"

#include <string>
#include <vector>

namespace mads_dash::ui {

/// The latest pair of snapshots of `x` and `y` with equal time at or before
/// `now`, as one curve (`n = min(len x, len y)` points). Returns false if
/// there is none.
bool curve_points(const data::Series &x, const data::Series &y, double now,
                  std::vector<double> &xs, std::vector<double> &ys);

/// True when `node` (an intermediate tree node such as `/testsource/xy`) has
/// exactly two direct Array children with equal latest length; sets their
/// keys in sorted order (x before y).
bool curve_pair(const data::Store &store, const std::string &node,
                std::string &x_key, std::string &y_key);

/// One curve from two equal-length arrays: the first source is X, the second
/// is Y. Shows the latest pair received together at or before `tl.now`.
class CurveView : public View {
public:
  using View::View;
  const char *type_name() const override { return "XY curve"; }
  bool accepts(data::Kind k) const override { return k == data::Kind::Array; }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;
  nlohmann::json options() const override;
  void set_options(const nlohmann::json &j) override;

private:
  int _style = 2; ///< 0 line, 1 markers, 2 line + markers
  int _marker_size = 4;
  float _color[3] = {0.12f, 0.47f, 0.71f};
  AutoScale _scale;
};

} // namespace mads_dash::ui
