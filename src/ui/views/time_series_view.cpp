#include "ui/views/time_series_view.hpp"

#include <implot.h>

#include <algorithm>
#include <vector>

namespace mads_dash::ui {

void TimeSeriesView::draw(const data::Store &store, const Timeline &tl) {
  if (!ImPlot::BeginPlot("##ts", ImVec2(-1, -1)))
    return;
  ImPlot::SetupAxes("t [s]", nullptr, 0,
                    tl.paused ? 0 : ImPlotAxisFlags_AutoFit);
  if (!tl.paused)
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
  ImPlot::EndPlot();
}

} // namespace mads_dash::ui
