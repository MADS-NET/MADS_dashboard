#pragma once

#include "ui/view.hpp"

#include <vector>

namespace mads_dash::ui {

/// Values of a Number series with t0 <= t <= t1 (bounds inclusive).
std::vector<double> window_values(const data::Series &s, double t0, double t1);

/// Histogram of each source's values in `[now - window, now]`, overlaid.
class HistogramView : public View {
public:
  using View::View;
  const char *type_name() const override { return "Histogram"; }
  bool accepts(data::Kind k) const override { return k == data::Kind::Number; }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;
  nlohmann::json options() const override;
  void set_options(const nlohmann::json &j) override;

private:
  int _bins = 30;
  bool _density = false;
};

} // namespace mads_dash::ui
