#include "ui/views/boxplot_view.hpp"

#include "ui/views/histogram_view.hpp"

#include <implot.h>

#include <algorithm>
#include <cmath>

namespace mads_dash::ui {

namespace {
/// Linear-interpolated quantile of sorted, non-empty values.
double quantile(const std::vector<double> &s, double p) {
  const double pos = p * double(s.size() - 1);
  const std::size_t i = (std::size_t)pos;
  if (i + 1 >= s.size())
    return s.back();
  return s[i] + (pos - double(i)) * (s[i + 1] - s[i]);
}

std::string last_segment(const std::string &key) {
  const auto p = key.find_last_of('/');
  return p == std::string::npos ? key : key.substr(p + 1);
}
} // namespace

BoxStats box_stats(std::vector<double> values) {
  BoxStats b;
  if (values.empty())
    return b;
  std::sort(values.begin(), values.end());
  b.q1 = quantile(values, 0.25);
  b.median = quantile(values, 0.5);
  b.q3 = quantile(values, 0.75);
  const double iqr = b.q3 - b.q1;
  const double lo_fence = b.q1 - 1.5 * iqr, hi_fence = b.q3 + 1.5 * iqr;
  b.lo = b.q1;
  b.hi = b.q3;
  for (double v : values) {
    if (v < lo_fence || v > hi_fence)
      b.outliers.push_back(v);
  }
  for (double v : values) {
    if (v >= lo_fence) {
      b.lo = v;
      break;
    }
  }
  for (auto it = values.rbegin(); it != values.rend(); ++it) {
    if (*it <= hi_fence) {
      b.hi = *it;
      break;
    }
  }
  return b;
}

void BoxplotView::draw(const data::Store &store, const Timeline &tl) {
  const int n = (int)_sources.size();
  // statistics over the common window, per source (empty = nothing drawn)
  std::vector<BoxStats> stats(n);
  std::vector<bool> has(n, false);
  double lo = INFINITY, hi = -INFINITY;
  for (int i = 0; i < n; ++i) {
    const auto it = store.series().find(_sources[i]);
    if (it == store.series().end())
      continue;
    auto vals = window_values(it->second, tl.begin(), tl.now);
    if (vals.empty())
      continue;
    stats[i] = box_stats(std::move(vals));
    has[i] = true;
    lo = std::min(lo, stats[i].lo);
    hi = std::max(hi, stats[i].hi);
    for (double o : stats[i].outliers) {
      lo = std::min(lo, o);
      hi = std::max(hi, o);
    }
  }
  _scale.begin_frame();
  if (_scale.on() && lo <= hi)
    _scale.y.update(lo, hi);

  if (!ImPlot::BeginPlot("##box", ImVec2(-1, -1)))
    return;
  std::vector<std::string> names;
  std::vector<const char *> labels;
  std::vector<double> pos;
  for (int i = 0; i < n; ++i) {
    names.push_back(last_segment(_sources[i]));
    pos.push_back(i);
  }
  for (const auto &s : names)
    labels.push_back(s.c_str());
  ImPlot::SetupAxes("", "", ImPlotAxisFlags_NoGridLines, 0);
  ImPlot::SetupAxisLimits(ImAxis_X1, -0.5, std::max(n, 1) - 0.5,
                          ImPlotCond_Always);
  if (_scale.on() && _scale.y.valid)
    ImPlot::SetupAxisLimits(ImAxis_Y1, _scale.y.lo, _scale.y.hi,
                            ImPlotCond_Always);
  if (n > 0)
    ImPlot::SetupAxisTicks(ImAxis_X1, pos.data(), n, labels.data());

  ImDrawList *dl = ImPlot::GetPlotDrawList();
  ImPlot::PushPlotClipRect();
  for (int i = 0; i < n; ++i) {
    if (!has[i])
      continue;
    const BoxStats &b = stats[i];
    const ImU32 col = ImGui::GetColorU32(ImPlot::GetColormapColor(i));
    const auto px = [&](double x, double y) {
      return ImPlot::PlotToPixels(x, y);
    };
    const ImVec2 tl_ = px(i - 0.25, b.q3), br = px(i + 0.25, b.q1);
    dl->AddRectFilled(tl_, br, (col & 0x00FFFFFF) | 0x60000000);
    dl->AddRect(tl_, br, col, 0.0f, 2.0f);
    dl->AddLine(px(i - 0.25, b.median), px(i + 0.25, b.median), col, 3);
    dl->AddLine(px(i, b.q3), px(i, b.hi), col, 1.5f);
    dl->AddLine(px(i, b.q1), px(i, b.lo), col, 1.5f);
    dl->AddLine(px(i - 0.1, b.hi), px(i + 0.1, b.hi), col, 1.5f);
    dl->AddLine(px(i - 0.1, b.lo), px(i + 0.1, b.lo), col, 1.5f);
    for (double o : b.outliers)
      dl->AddCircle(px(i, o), 3, col);
  }
  ImPlot::PopPlotClipRect();
  _scale.set_hovered(ImPlot::IsPlotHovered() ||
                     ImPlot::IsAxisHovered(ImAxis_X1) ||
                     ImPlot::IsAxisHovered(ImAxis_Y1));
  ImPlot::EndPlot();
}

} // namespace mads_dash::ui
