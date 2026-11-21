#include "ui/views/histogram_view.hpp"

#include <implot.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace mads_dash::ui {

std::vector<double> window_values(const data::Series &s, double t0, double t1) {
  const auto a = std::lower_bound(s.t.begin(), s.t.end(), t0) - s.t.begin();
  const auto b = std::upper_bound(s.t.begin(), s.t.end(), t1) - s.t.begin();
  if (b <= a)
    return {};
  return {s.v.begin() + a, s.v.begin() + b};
}

void HistogramView::draw_options() {
  ImGui::SetNextItemWidth(150);
  ImGui::SliderInt("bins", &_bins, 5, 200);
  ImGui::Checkbox("density", &_density);
}

void HistogramView::draw(const data::Store &store, const Timeline &tl) {
  // Values in the common window; one value range for all sources, so the
  // overlaid histograms share their bins.
  std::vector<std::pair<const std::string *, std::vector<double>>> data;
  double lo = INFINITY, hi = -INFINITY;
  for (const auto &key : _sources) {
    const auto it = store.series().find(key);
    if (it == store.series().end() || it->second.kind != data::Kind::Number)
      continue;
    auto vals = window_values(it->second, tl.begin(), tl.now);
    if (vals.empty())
      continue;
    auto [mn, mx] = std::minmax_element(vals.begin(), vals.end());
    lo = std::min(lo, *mn);
    hi = std::max(hi, *mx);
    data.emplace_back(&key, std::move(vals));
  }
  if (hi == lo) { // a single value: give the bins some width
    lo -= 0.5;
    hi += 0.5;
  }
  _scale.begin_frame();
  if (_scale.on() && !data.empty()) {
    double top = 0;
    const double width = (hi - lo) / _bins;
    for (const auto &[key, vals] : data) {
      std::vector<int> counts(_bins, 0);
      for (double v : vals)
        ++counts[std::clamp(int((v - lo) / width), 0, _bins - 1)];
      const double c = *std::max_element(counts.begin(), counts.end());
      top = std::max(top, _density ? c / (vals.size() * width) : c);
    }
    _scale.x.update(lo, hi);
    _scale.y.update(0, top);
  }

  if (!ImPlot::BeginPlot("##hist", ImVec2(-1, -1)))
    return;
  ImPlot::SetupAxes("value", _density ? "density" : "count");
  if (_scale.on() && _scale.x.valid) {
    ImPlot::SetupAxisLimits(ImAxis_X1, _scale.x.lo, _scale.x.hi,
                            ImPlotCond_Always);
    // counts start at 0: only the top is sticky
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, _scale.y.hi, ImPlotCond_Always);
  }
  for (const auto &[key, vals] : data) {
    ImPlotSpec spec;
    spec.FillAlpha = 0.5f;
    if (_density)
      spec.Flags = ImPlotHistogramFlags_Density;
    ImPlot::PlotHistogram(key->c_str(), vals.data(), (int)vals.size(), _bins,
                          1.0, ImPlotRange(lo, hi), spec);
  }
  _scale.set_hovered(ImPlot::IsPlotHovered() ||
                     ImPlot::IsAxisHovered(ImAxis_X1) ||
                     ImPlot::IsAxisHovered(ImAxis_Y1));
  ImPlot::EndPlot();
}

nlohmann::json HistogramView::options() const {
  return {{"bins", _bins}, {"density", _density}};
}

void HistogramView::set_options(const nlohmann::json &j) {
  if (auto it = j.find("bins"); it != j.end() && it->is_number_integer())
    _bins = std::clamp(it->get<int>(), 5, 200);
  if (auto it = j.find("density"); it != j.end() && it->is_boolean())
    _density = it->get<bool>();
}

} // namespace mads_dash::ui
