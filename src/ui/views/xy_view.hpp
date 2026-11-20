#pragma once

#include "ui/view.hpp"

namespace mads_dash::ui {

/// XY plot, one view for scatter, trace and 2D trajectory (PLAN §4.3).
/// Sources, by count and kind:
///  - two Arrays of equal length (X, Y): one curve per snapshot;
///  - two Numbers (X, Y): a trace of (x(t), y(t));
///  - one Array [x, y, ...]: a trajectory of its first two elements.
/// Shows the last `history` snapshots/samples up to the timeline's `now`,
/// older ones faded when `fade` is on.
class XYView : public View {
public:
  using View::View;
  const char *type_name() const override { return "XY"; }
  bool accepts(data::Kind k) const override {
    return k == data::Kind::Number || k == data::Kind::Array;
  }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;
  nlohmann::json options() const override;
  void set_options(const nlohmann::json &j) override;

private:
  int _history = 200;
  bool _fade = true;
  bool _markers = false;
};

} // namespace mads_dash::ui
