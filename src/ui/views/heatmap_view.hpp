#pragma once

#include "ui/view.hpp"

#include <nlohmann/json.hpp>

#include <utility>
#include <vector>

namespace mads_dash::ui {

/// Flattens a JSON array of rows into `out` (row-major). Returns false, and
/// leaves `out` unspecified, for empty, ragged or non-numeric input.
bool matrix_values(const nlohmann::json &m, std::vector<double> &out, int &rows,
                   int &cols);

/// (min, max) of finite values; (0, 1) if there are none; widened if equal.
std::pair<double, double> value_range(const std::vector<double> &v);

/// Heatmap of the latest matrix of one source (a second source replaces the
/// first: only the last one is drawn).
// shortcut: matrices keep no history (Series::last only), so the heatmap does
// not follow the timeline when paused; accepted for M2.
class HeatmapView : public View {
public:
  using View::View;
  const char *type_name() const override { return "Heatmap"; }
  bool accepts(data::Kind k) const override { return k == data::Kind::Matrix; }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;
  nlohmann::json options() const override;
  void set_options(const nlohmann::json &j) override;

private:
  bool _auto_range = true;
  double _min = 0;
  double _max = 1;
  int _cmap = 0; ///< index into Viridis, Plasma, Hot
};

} // namespace mads_dash::ui
