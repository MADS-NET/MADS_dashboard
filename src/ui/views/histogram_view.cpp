#include "ui/views/histogram_view.hpp"

#include <implot.h>

#include <algorithm>

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
  if (!ImPlot::BeginPlot("##hist", ImVec2(-1, -1)))
    return;
  ImPlot::SetupAxes("value", _density ? "density" : "count",
                    tl.paused ? 0 : ImPlotAxisFlags_AutoFit,
                    tl.paused ? 0 : ImPlotAxisFlags_AutoFit);
  for (const auto &key : _sources) {
    const auto it = store.series().find(key);
    if (it == store.series().end() || it->second.kind != data::Kind::Number)
      continue;
    const auto vals = window_values(it->second, tl.now - tl.window, tl.now);
    if (vals.empty())
      continue;
    ImPlotSpec spec;
    spec.FillAlpha = 0.5f;
    if (_density)
      spec.Flags = ImPlotHistogramFlags_Density;
    ImPlot::PlotHistogram(key.c_str(), vals.data(), (int)vals.size(), _bins,
                          1.0, ImPlotRange(), spec);
  }
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
