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
  if (!ImPlot::BeginPlot("##ts", ImVec2(-1, -below - (_cursor ? 4.f : 0.f))))
    return;
  ImPlot::SetupAxes("t [s]", nullptr, 0,
                    tl.paused ? 0 : ImPlotAxisFlags_AutoFit);
  if (!tl.paused || tl.jumped)
    ImPlot::SetupAxisLimits(ImAxis_X1, tl.now - tl.window, tl.now,
                            ImPlotCond_Always);
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
    ImPlot::PlotLine(key.c_str(), s.t.data() + first, s.v.data() + first,
                     count / step, spec);
  }
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
    ImPlot::DragLineX(0, &_cursor_t, ImVec4(1, 1, 0, 1));
  }
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
  return {{"cursor", _cursor}, {"snap", _snap}};
}

void TimeSeriesView::set_options(const nlohmann::json &j) {
  if (auto it = j.find("cursor"); it != j.end() && it->is_boolean())
    _cursor = it->get<bool>();
  if (auto it = j.find("snap"); it != j.end() && it->is_boolean())
    _snap = it->get<bool>();
}

} // namespace mads_dash::ui
