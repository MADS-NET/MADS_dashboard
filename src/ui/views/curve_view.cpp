#include "ui/views/curve_view.hpp"

#include <implot.h>

#include <algorithm>
#include <cmath>

namespace mads_dash::ui {

namespace {
const data::Series *find(const data::Store &store, const std::string &key) {
  const auto it = store.series().find(key);
  return it == store.series().end() ? nullptr : &it->second;
}
const char *const kStyles[] = {"line", "markers", "line + markers"};
} // namespace

bool curve_points(const data::Series &x, const data::Series &y, double now,
                  std::vector<double> &xs, std::vector<double> &ys) {
  // newest x snapshot at or before `now` that has a y snapshot of equal time
  auto xe =
      std::upper_bound(x.snaps.begin(), x.snaps.end(), now,
                       [](double t, const auto &s) { return t < s.first; });
  while (xe != x.snaps.begin()) {
    --xe;
    const auto ye =
        std::lower_bound(y.snaps.begin(), y.snaps.end(), xe->first,
                         [](const auto &s, double t) { return s.first < t; });
    if (ye != y.snaps.end() && ye->first == xe->first) {
      const std::size_t n = std::min(xe->second.size(), ye->second.size());
      xs.assign(xe->second.begin(), xe->second.begin() + n);
      ys.assign(ye->second.begin(), ye->second.begin() + n);
      return true;
    }
  }
  return false;
}

bool curve_pair(const data::Store &store, const std::string &node,
                std::string &x_key, std::string &y_key) {
  const std::string prefix = node + "/";
  std::vector<const std::string *> keys;
  std::size_t len = 0;
  for (const auto &[key, s] : store.series()) { // map order: sorted
    if (key.compare(0, prefix.size(), prefix) != 0 ||
        key.find('/', prefix.size()) != std::string::npos ||
        s.kind != data::Kind::Array)
      continue;
    if (s.snaps.empty())
      return false;
    if (!keys.empty() && s.snaps.back().second.size() != len)
      return false;
    len = s.snaps.back().second.size();
    keys.push_back(&key);
  }
  if (keys.size() != 2)
    return false;
  x_key = *keys[0];
  y_key = *keys[1];
  return true;
}

void CurveView::draw_options() {
  ImGui::SetNextItemWidth(150);
  ImGui::Combo("style", &_style, kStyles, 3);
  ImGui::SetNextItemWidth(150);
  ImGui::SliderInt("marker size", &_marker_size, 1, 10);
  ImGui::ColorEdit3("color", _color, ImGuiColorEditFlags_NoInputs);
}

void CurveView::draw(const data::Store &store, const Timeline &tl) {
  const data::Series *sx =
      _sources.size() > 0 ? find(store, _sources[0]) : nullptr;
  const data::Series *sy =
      _sources.size() > 1 ? find(store, _sources[1]) : nullptr;
  std::vector<double> xs, ys;
  const bool have = sx && sy && sx->kind == data::Kind::Array &&
                    sy->kind == data::Kind::Array &&
                    curve_points(*sx, *sy, tl.now, xs, ys) && !xs.empty();

  _scale.begin_frame();
  if (_scale.on() && have) {
    const auto [x0, x1] = std::minmax_element(xs.begin(), xs.end());
    const auto [y0, y1] = std::minmax_element(ys.begin(), ys.end());
    _scale.x.update(*x0, *x1);
    _scale.y.update(*y0, *y1);
  }

  if (!ImPlot::BeginPlot("##curve", ImVec2(-1, -1)))
    return;
  const std::string xl = _sources.size() > 0 ? _sources[0] : std::string();
  const std::string yl = _sources.size() > 1 ? _sources[1] : std::string();
  ImPlot::SetupAxes(xl.c_str(), yl.c_str());
  if (_scale.on() && _scale.x.valid && _scale.y.valid) {
    ImPlot::SetupAxisLimits(ImAxis_X1, _scale.x.lo, _scale.x.hi,
                            ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, _scale.y.lo, _scale.y.hi,
                            ImPlotCond_Always);
  }
  if (have) {
    const ImVec4 col(_color[0], _color[1], _color[2], 1);
    ImPlotSpec spec;
    spec.LineColor = col;
    spec.MarkerFillColor = col;
    spec.MarkerLineColor = col;
    spec.MarkerSize = (float)_marker_size;
    if (_style != 0)
      spec.Marker = ImPlotMarker_Circle;
    if (_style == 1)
      ImPlot::PlotScatter("curve", xs.data(), ys.data(), (int)xs.size(), spec);
    else
      ImPlot::PlotLine("curve", xs.data(), ys.data(), (int)xs.size(), spec);
  }
  _scale.set_hovered(ImPlot::IsPlotHovered() ||
                     ImPlot::IsAxisHovered(ImAxis_X1) ||
                     ImPlot::IsAxisHovered(ImAxis_Y1));
  ImPlot::EndPlot();
}

nlohmann::json CurveView::options() const {
  return {{"style", _style},
          {"marker_size", _marker_size},
          {"color", {_color[0], _color[1], _color[2]}}};
}

void CurveView::set_options(const nlohmann::json &j) {
  if (auto it = j.find("style"); it != j.end() && it->is_number_integer())
    _style = std::clamp(it->get<int>(), 0, 2);
  if (auto it = j.find("marker_size"); it != j.end() && it->is_number_integer())
    _marker_size = std::clamp(it->get<int>(), 1, 10);
  if (auto it = j.find("color");
      it != j.end() && it->is_array() && it->size() == 3 &&
      std::all_of(it->begin(), it->end(),
                  [](const auto &v) { return v.is_number(); }))
    for (int i = 0; i < 3; ++i)
      _color[i] = std::clamp((*it)[i].template get<float>(), 0.0f, 1.0f);
}

} // namespace mads_dash::ui
