#include "ui/views/heatmap_view.hpp"

#include <implot.h>

#include <algorithm>
#include <cmath>

namespace mads_dash::ui {

namespace {
constexpr ImPlotColormap kMaps[] = {ImPlotColormap_Viridis,
                                    ImPlotColormap_Plasma, ImPlotColormap_Hot};
constexpr const char *kMapNames[] = {"Viridis", "Plasma", "Hot"};
} // namespace

bool matrix_values(const nlohmann::json &m, std::vector<double> &out, int &rows,
                   int &cols) {
  out.clear();
  rows = cols = 0;
  if (!m.is_array() || m.empty() || !m[0].is_array() || m[0].empty())
    return false;
  rows = (int)m.size();
  cols = (int)m[0].size();
  for (const auto &row : m) {
    if (!row.is_array() || (int)row.size() != cols)
      return false;
    for (const auto &x : row) {
      if (!x.is_number())
        return false;
      out.push_back(x.get<double>());
    }
  }
  return true;
}

std::pair<double, double> value_range(const std::vector<double> &v) {
  double lo = INFINITY, hi = -INFINITY;
  for (double x : v)
    if (std::isfinite(x)) {
      lo = std::min(lo, x);
      hi = std::max(hi, x);
    }
  if (lo > hi)
    return {0, 1};
  if (lo == hi)
    return {lo - 0.5, hi + 0.5};
  return {lo, hi};
}

void HeatmapView::draw_options() {
  ImGui::Checkbox("auto range", &_auto_range);
  if (!_auto_range) {
    ImGui::SetNextItemWidth(100);
    ImGui::InputDouble("min", &_min);
    ImGui::SetNextItemWidth(100);
    ImGui::InputDouble("max", &_max);
  }
  ImGui::SetNextItemWidth(100);
  ImGui::Combo("colormap", &_cmap, kMapNames, 3);
}

void HeatmapView::draw(const data::Store &store, const Timeline &) {
  std::vector<double> vals;
  int rows = 0, cols = 0;
  if (!_sources.empty()) {
    auto it = store.series().find(_sources.back());
    if (it != store.series().end() &&
        !matrix_values(it->second.last, vals, rows, cols))
      vals.clear();
  }
  if (vals.empty()) {
    ImGui::TextUnformatted("Waiting for a numeric matrix...");
    return;
  }
  auto [lo, hi] = _auto_range ? value_range(vals) : std::pair(_min, _max);
  if (!(hi > lo))
    hi = lo + 1;
  const ImPlotColormap cmap = kMaps[std::clamp(_cmap, 0, 2)];
  ImPlot::PushColormap(cmap);
  if (ImPlot::BeginPlot("##heatmap", ImVec2(-80, -1), ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit,
                      ImPlotAxisFlags_AutoFit);
    ImPlot::PlotHeatmap("matrix", vals.data(), rows, cols, lo, hi, nullptr);
    ImPlot::EndPlot();
  }
  ImGui::SameLine();
  ImPlot::ColormapScale("##scale", lo, hi, ImVec2(70, -1), "%g", 0, cmap);
  ImPlot::PopColormap();
}

} // namespace mads_dash::ui
