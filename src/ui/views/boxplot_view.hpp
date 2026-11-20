#pragma once

#include "ui/view.hpp"

#include <limits>
#include <map>
#include <vector>

namespace mads_dash::ui {

/// Box statistics: quartiles (linear-interpolated), whiskers at the most
/// extreme values within 1.5 IQR of the quartiles, outliers beyond them.
struct BoxStats {
  double q1 = std::numeric_limits<double>::quiet_NaN();
  double median = std::numeric_limits<double>::quiet_NaN();
  double q3 = std::numeric_limits<double>::quiet_NaN();
  double lo = std::numeric_limits<double>::quiet_NaN();
  double hi = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> outliers;
};

/// Computes the box statistics; an empty input gives NaNs and no outliers.
BoxStats box_stats(std::vector<double> values);

/// Cumulative boxplot: one box per source over all samples seen since the
/// view was created or last reset (not limited to the window).
class BoxplotView : public View {
public:
  using View::View;
  const char *type_name() const override { return "Boxplot"; }
  bool accepts(data::Kind k) const override { return k == data::Kind::Number; }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;

private:
  struct State {
    double last_t = -std::numeric_limits<double>::infinity();
    // shortcut: values are kept as doubles and grow without bound (about 8 MB
    // per million samples); switch to a streaming quantile estimator (P2 or
    // t-digest) when that becomes a problem. box_stats() also re-sorts all
    // values on every frame with new data: same fix.
    std::vector<double> values;
    BoxStats stats;
    bool dirty = false;
  };
  std::map<std::string, State> _state;
};

} // namespace mads_dash::ui
