#include "ui/views/boxplot_view.hpp"

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

void BoxplotView::draw_options() {
  if (ImGui::MenuItem("Reset"))
    _state.clear();
}

void BoxplotView::draw(const data::Store &store, const Timeline &tl) {
  // append every new sample (t <= now) to the per-source running state
  std::erase_if(_state, [&](const auto &kv) {
    return std::find(_sources.begin(), _sources.end(), kv.first) ==
           _sources.end();
  });
  for (const auto &key : _sources) {
    auto it = store.series().find(key);
    if (it == store.series().end())
      continue;
    const auto &s = it->second;
    auto &st = _state[key];
    auto i = std::upper_bound(s.t.begin(), s.t.end(), st.last_t) - s.t.begin();
    for (; i < (std::ptrdiff_t)s.t.size() && s.t[i] <= tl.now; ++i) {
      st.values.push_back(s.v[i]);
      st.last_t = s.t[i];
      st.dirty = true;
    }
    if (st.dirty) {
      st.stats = box_stats(st.values);
      st.dirty = false;
    }
  }

  if (!ImPlot::BeginPlot("##box", ImVec2(-1, -1)))
    return;
  const int n = (int)_sources.size();
  std::vector<std::string> names;
  std::vector<const char *> labels;
  std::vector<double> pos;
  for (int i = 0; i < n; ++i) {
    names.push_back(last_segment(_sources[i]));
    pos.push_back(i);
  }
  for (const auto &s : names)
    labels.push_back(s.c_str());
  ImPlot::SetupAxes("", "", ImPlotAxisFlags_NoGridLines,
                    tl.paused ? 0 : ImPlotAxisFlags_AutoFit);
  ImPlot::SetupAxisLimits(ImAxis_X1, -0.5, std::max(n, 1) - 0.5,
                          ImPlotCond_Always);
  if (n > 0)
    ImPlot::SetupAxisTicks(ImAxis_X1, pos.data(), n, labels.data());

  ImDrawList *dl = ImPlot::GetPlotDrawList();
  ImPlot::PushPlotClipRect();
  for (int i = 0; i < n; ++i) {
    auto it = _state.find(_sources[i]);
    if (it == _state.end() || it->second.values.empty())
      continue;
    const BoxStats &b = it->second.stats;
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
  ImPlot::EndPlot();
}

} // namespace mads_dash::ui
