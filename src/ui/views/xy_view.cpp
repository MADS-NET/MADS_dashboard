#include "ui/views/xy_view.hpp"

#include <implot.h>

#include <algorithm>
#include <utility>

namespace mads_dash::ui {

namespace {
using data::Kind;
using data::Series;

const Series *find(const data::Store &store, const std::string &key) {
  auto it = store.series().find(key);
  return it == store.series().end() ? nullptr : &it->second;
}

/// Index one past the last element with time <= now.
template <class C, class T> std::size_t upto(const C &c, double now, T time) {
  return std::upper_bound(
             c.begin(), c.end(), now,
             [&](double n, const auto &e) { return n < time(e); }) -
         c.begin();
}

/// Draws a polyline in `chunks` pieces of rising opacity (oldest faintest).
void draw_trace(const char *label, const std::vector<double> &xs,
                const std::vector<double> &ys, bool fade, bool markers,
                ImVec4 col) {
  const int n = (int)xs.size();
  const int chunks = fade ? std::min(8, std::max(1, n / 2)) : 1;
  for (int c = 0; c < chunks; ++c) {
    const int a = c * n / chunks, b = std::min(n, (c + 1) * n / chunks + 1);
    ImPlotSpec spec;
    spec.LineColor = col;
    spec.LineColor.w = fade ? 0.15f + 0.85f * (c + 1) / chunks : 1.0f;
    if (markers)
      spec.Marker = ImPlotMarker_Circle;
    if (c != chunks - 1)
      spec.Flags = ImPlotItemFlags_NoLegend;
    ImPlot::PlotLine(label, xs.data() + a, ys.data() + a, b - a, spec);
  }
}
} // namespace

void XYView::draw_options() {
  ImGui::SetNextItemWidth(150);
  ImGui::SliderInt("history", &_history, 1, 2000, "%d",
                   ImGuiSliderFlags_Logarithmic);
  ImGui::Checkbox("fade older", &_fade);
  ImGui::Checkbox("markers", &_markers);
  if (_sources.size() == 2 && ImGui::MenuItem("Swap X / Y"))
    std::swap(_sources[0], _sources[1]);
}

void XYView::draw(const data::Store &store, const Timeline &tl) {
  const Series *sx = _sources.size() > 0 ? find(store, _sources[0]) : nullptr;
  const Series *sy = _sources.size() > 1 ? find(store, _sources[1]) : nullptr;
  if (!ImPlot::BeginPlot("##xy", ImVec2(-1, -1)))
    return;
  const auto x_label = _sources.size() > 0 ? _sources[0] : std::string();
  const auto y_label = _sources.size() > 1 ? _sources[1] : std::string();
  ImPlot::SetupAxes(x_label.c_str(), y_label.c_str(),
                    tl.paused ? 0 : ImPlotAxisFlags_AutoFit,
                    tl.paused ? 0 : ImPlotAxisFlags_AutoFit);
  const ImVec4 col = ImPlot::GetColormapColor(0);
  const auto snap_t = [](const auto &s) { return s.first; };
  std::vector<double> xs, ys;

  if (sx && sy && sx->kind == Kind::Array && sy->kind == Kind::Array) {
    // one curve per snapshot pair with equal time (same message)
    const std::size_t end = upto(sx->snaps, tl.now, snap_t);
    const std::size_t begin = end > (std::size_t)_history ? end - _history : 0;
    for (std::size_t i = begin; i < end; ++i) {
      const auto &[t, x] = sx->snaps[i];
      const std::size_t j = upto(sy->snaps, t, snap_t);
      if (j == 0 || sy->snaps[j - 1].first != t)
        continue;
      const auto &y = sy->snaps[j - 1].second;
      const int n = (int)std::min(x.size(), y.size());
      ImPlotSpec spec;
      spec.LineColor = col;
      spec.LineColor.w =
          _fade ? 0.1f + 0.9f * float(i - begin + 1) / float(end - begin) : 1;
      if (_markers)
        spec.Marker = ImPlotMarker_Circle;
      if (i + 1 != end)
        spec.Flags = ImPlotItemFlags_NoLegend;
      ImPlot::PlotLine("xy", x.data(), y.data(), n, spec);
    }
  } else if (sx && sy && sx->kind == Kind::Number && sy->kind == Kind::Number) {
    // trace of samples with equal time
    const std::size_t end =
        std::upper_bound(sx->t.begin(), sx->t.end(), tl.now) - sx->t.begin();
    const std::size_t begin = end > (std::size_t)_history ? end - _history : 0;
    for (std::size_t i = begin; i < end; ++i) {
      auto it = std::lower_bound(sy->t.begin(), sy->t.end(), sx->t[i]);
      if (it == sy->t.end() || *it != sx->t[i])
        continue;
      xs.push_back(sx->v[i]);
      ys.push_back(sy->v[it - sy->t.begin()]);
    }
    draw_trace("trace", xs, ys, _fade, _markers, col);
  } else if (sx && !sy && sx->kind == Kind::Array) {
    // trajectory of [x, y, ...]
    const std::size_t end = upto(sx->snaps, tl.now, snap_t);
    const std::size_t begin = end > (std::size_t)_history ? end - _history : 0;
    for (std::size_t i = begin; i < end; ++i) {
      const auto &a = sx->snaps[i].second;
      if (a.size() < 2)
        continue;
      xs.push_back(a[0]);
      ys.push_back(a[1]);
    }
    draw_trace("trajectory", xs, ys, _fade, _markers, col);
  } else {
    ImPlot::PlotText("XY needs two equal-length arrays, two numbers, or one "
                     "array [x, y, ...]",
                     0.5, 0.5);
  }
  ImPlot::EndPlot();
}

} // namespace mads_dash::ui
