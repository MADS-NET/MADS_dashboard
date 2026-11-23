#pragma once

#include "ui/auto_scale.hpp"
#include "ui/view.hpp"

namespace mads_dash::ui {

/// Value of `s` at time `t`: linear interpolation, clamped to the first and
/// last sample, NaN for an empty series.
double value_at(const data::Series &s, double t);

/// Time of the sample of `s` nearest to `t` (NaN for an empty series).
double snap_time(const data::Series &s, double t);

/// Strip chart of numbers and bools over the timeline window; strings are
/// drawn as labelled event lines.
class TimeSeriesView : public View {
public:
  using View::View;
  const char *type_name() const override { return "Time series"; }
  bool accepts(data::Kind k) const override {
    return k == data::Kind::Number || k == data::Kind::Bool ||
           k == data::Kind::String;
  }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;
  nlohmann::json options() const override;
  void set_options(const nlohmann::json &j) override;

private:
  bool _cursor = false;
  bool _snap = true;
  bool _markers = false;
  bool _cursor_set = false; ///< position initialised since the cursor was on
  double _cursor_t = 0;
  AutoScale _scale;
};

} // namespace mads_dash::ui
