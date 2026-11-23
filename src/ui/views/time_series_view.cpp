#include "ui/views/time_series_view.hpp"

#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace mads_dash::ui {

double value_at(const data::Series &s, double t) {
  if (s.t.empty() || s.v.size() != s.t.size())
    return std::nan("");
  if (t <= s.t.front())
    return s.v.front();
  if (t >= s.t.back())
    return s.v.back();
  const auto hi = std::lower_bound(s.t.begin(), s.t.end(), t);
  const size_t i = (size_t)(hi - s.t.begin());
  const double t0 = s.t[i - 1], t1 = s.t[i];
  if (t1 <= t0)
    return s.v[i];
  return s.v[i - 1] + (s.v[i] - s.v[i - 1]) * (t - t0) / (t1 - t0);
}

double snap_time(const data::Series &s, double t) {
  if (s.t.empty())
    return std::nan("");
  const auto hi = std::lower_bound(s.t.begin(), s.t.end(), t);
  if (hi == s.t.begin())
    return *hi;
  if (hi == s.t.end())
    return s.t.back();
  return (t - *(hi - 1) <= *hi - t) ? *(hi - 1) : *hi;
}

void TimeSeriesView::draw_options() {
  ImGui::Checkbox("markers", &_markers);
  ImGui::Checkbox("cursor", &_cursor);
  if (_cursor)
    ImGui::Checkbox("snap", &_snap);
}

void TimeSeriesView::draw(const data::Store &store, const Timeline &tl) {
  auto numeric = [&](const std::string &key) -> const data::Series * {
    auto it = store.series().find(key);
    if (it == store.series().end() || it->second.t.empty() ||
        it->second.kind == data::Kind::String)
      return nullptr;
    return &it->second;
  };
  const float line = ImGui::GetTextLineHeightWithSpacing();
  float below = 0;
  if (_cursor) {
    int n = 1; // the Copy row
    for (const auto &key : _sources)
      n += numeric(key) != nullptr;
    below = line * (float)n;
  } else {
    _cursor_set = false;
  }
  // Autoscale: x follows the timeline window, y is sticky over the data in it.
  _scale.begin_frame();
  const double x0 = tl.begin(), x1 = tl.now;
  bool clock = !_sources.empty();
  if (_scale.on()) {
    double ymin = INFINITY, ymax = -INFINITY;
    for (const auto &key : _sources)
      if (const data::Series *s = numeric(key)) {
        // shortcut: O(samples in window) per frame; keep a running min/max
        // per series if very long windows at kHz rates lag
        auto a = std::lower_bound(s->t.begin(), s->t.end(), x0);
        auto b = std::upper_bound(a, s->t.end(), x1);
        if (a != b) {
          auto [mn, mx] =
              std::minmax_element(s->v.begin() + (a - s->t.begin()),
                                  s->v.begin() + (b - s->t.begin()));
          ymin = std::min(ymin, *mn);
          ymax = std::max(ymax, *mx);
        }
      }
    _scale.y.update(ymin, ymax);
  }
  for (const auto &key : _sources)
    clock = clock && store.time_base(data::Store::topic_of(key)).clock;

  // a height of -0 would mean "default height", not "fill the window"
  const float height = _cursor ? -below - 4.f : -1.f;
  if (!ImPlot::BeginPlot("##ts", ImVec2(-1, height)))
    return;
  ImPlot::SetupAxes(clock ? nullptr : "t [s]", nullptr);
  if (clock)
    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
  if (_scale.on()) {
    ImPlot::SetupAxisLimits(ImAxis_X1, x0, x1, ImPlotCond_Always);
    if (_scale.y.valid)
      ImPlot::SetupAxisLimits(ImAxis_Y1, _scale.y.lo, _scale.y.hi,
                              ImPlotCond_Always);
  }
  const ImPlotRect lim = ImPlot::GetPlotLimits(); // valid after setup
  const int pixels = std::max(1, (int)ImPlot::GetPlotSize().x);

  for (const auto &key : _sources) {
    auto it = store.series().find(key);
    if (it == store.series().end())
      continue;
    const data::Series &s = it->second;
    if (s.kind == data::Kind::String) {
      // events: vertical lines labelled with the new value
      std::vector<double> ts;
      for (const auto &[t, text] : s.events) {
        if (t < lim.X.Min || t > lim.X.Max)
          continue;
        ts.push_back(t);
        ImPlot::PlotText(text.c_str(), t, lim.Y.Max, ImVec2(4, 10));
      }
      ImPlot::PlotInfLines(key.c_str(), ts.data(), (int)ts.size());
      continue;
    }
    if (s.t.empty())
      continue;
    // only the visible range (+1 sample each side), decimated to ~2 pts/pixel
    auto lo = std::lower_bound(s.t.begin(), s.t.end(), lim.X.Min);
    auto hi = std::upper_bound(lo, s.t.end(), lim.X.Max);
    if (lo != s.t.begin())
      --lo;
    if (hi != s.t.end())
      ++hi;
    const int first = (int)(lo - s.t.begin());
    const int count = (int)(hi - lo);
    // shortcut: stride decimation can hide spikes; switch to min/max buckets
    // in M2 if that matters
    const int step = std::max(1, count / (2 * pixels));
    ImPlotSpec spec;
    spec.Stride = step * (int)sizeof(double);
    if (_markers) // on the drawn points, i.e. after decimation
      spec.Marker = ImPlotMarker_Circle;
    ImPlot::PlotLine(key.c_str(), s.t.data() + first, s.v.data() + first,
                     count / step, spec);
  }
  bool cursor_hovered = false, cursor_held = false;
  if (_cursor) {
    if (!_cursor_set) {
      _cursor_t = 0.5 * (lim.X.Min + lim.X.Max);
      _cursor_set = true;
    }
    if (_snap) {
      for (const auto &key : _sources)
        if (const data::Series *s = numeric(key)) {
          _cursor_t = snap_time(*s, _cursor_t);
          break;
        }
    }
    ImPlot::DragLineX(0, &_cursor_t, ImVec4(1, 1, 0, 1), 1, 0, nullptr,
                      &cursor_hovered, &cursor_held);
  }
  // dragging the cursor is not a pan: it must not release the autoscale
  _scale.set_hovered(!cursor_hovered && !cursor_held &&
                     (ImPlot::IsPlotHovered() ||
                      ImPlot::IsAxisHovered(ImAxis_X1) ||
                      ImPlot::IsAxisHovered(ImAxis_Y1)));
  ImPlot::EndPlot();

  if (!_cursor)
    return;
  char tbuf[64];
  std::snprintf(tbuf, sizeof tbuf, "%.6f", _cursor_t);
  std::string text = tbuf;
  for (const auto &key : _sources) {
    const data::Series *s = numeric(key);
    if (!s)
      continue;
    const double v = value_at(*s, _cursor_t); // exact at a snapped sample
    ImGui::Text("%s = %.6g", key.c_str(), v);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    text += "\t" + key + "=" + buf;
  }
  if (ImGui::SmallButton("Copy"))
    ImGui::SetClipboardText(text.c_str());
}

nlohmann::json TimeSeriesView::options() const {
  return {{"cursor", _cursor}, {"snap", _snap}, {"markers", _markers}};
}

void TimeSeriesView::set_options(const nlohmann::json &j) {
  if (auto it = j.find("cursor"); it != j.end() && it->is_boolean())
    _cursor = it->get<bool>();
  if (auto it = j.find("snap"); it != j.end() && it->is_boolean())
    _snap = it->get<bool>();
  if (auto it = j.find("markers"); it != j.end() && it->is_boolean())
    _markers = it->get<bool>();
}

} // namespace mads_dash::ui
