#pragma once

#include "ui/auto_scale.hpp"
#include "ui/view.hpp"

#include <limits>
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

/// Boxplot: one box per source over its samples in the common window.
class BoxplotView : public View {
public:
  using View::View;
  const char *type_name() const override { return "Boxplot"; }
  bool accepts(data::Kind k) const override { return k == data::Kind::Number; }
  void draw(const data::Store &store, const Timeline &tl) override;

private:
  AutoScale _scale;
};

} // namespace mads_dash::ui
