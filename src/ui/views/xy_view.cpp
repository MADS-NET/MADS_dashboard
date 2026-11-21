#include "ui/views/xy_view.hpp"

#include <implot.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace mads_dash::ui {

namespace {
using data::Kind;
using data::Series;

const Series *find(const data::Store &store, const std::string &key) {
  auto it = store.series().find(key);
  return it == store.series().end() ? nullptr : &it->second;
}

/// Calls f(time, value) for the samples of axis `a` with time in [t0, t1].
template <class F>
void each_sample(const Series &s, const AxisSource &a, double t0, double t1,
                 F &&f) {
  if (s.kind == Kind::Number) {
    auto lo = std::lower_bound(s.t.begin(), s.t.end(), t0) - s.t.begin();
    for (auto i = (std::size_t)lo; i < s.t.size() && s.t[i] <= t1; ++i)
      f(s.t[i], s.v[i]);
  } else if (s.kind == Kind::Array && a.index >= 0) {
    auto it = std::partition_point(s.snaps.begin(), s.snaps.end(),
                                   [&](const auto &p) { return p.first < t0; });
    for (; it != s.snaps.end() && it->first <= t1; ++it)
      if ((std::size_t)a.index < it->second.size())
        f(it->first, it->second[a.index]);
  }
}

/// The value of axis `a` at exactly time `t`.
bool value_at_time(const Series &s, const AxisSource &a, double t, double &v) {
  bool found = false;
  each_sample(s, a, t, t, [&](double, double x) {
    v = x;
    found = true;
  });
  return found;
}
} // namespace

bool TraceSeries::complete() const {
  return std::all_of(axes.begin(), axes.end(),
                     [](const AxisSource &a) { return !a.key.empty(); });
}

TracePoints trace_points(const data::Store &store,
                         const std::vector<AxisSource> &axes, double t0,
                         double t1) {
  TracePoints out;
  out.v.resize(axes.size());
  std::vector<const Series *> ss;
  for (const auto &a : axes)
    ss.push_back(find(store, a.key));
  if (ss.empty() || std::find(ss.begin(), ss.end(), nullptr) != ss.end())
    return out;
  std::vector<double> row(axes.size());
  each_sample(*ss[0], axes[0], t0, t1, [&](double t, double x) {
    row[0] = x;
    for (std::size_t d = 1; d < axes.size(); ++d)
      if (!value_at_time(*ss[d], axes[d], t, row[d]))
        return;
    for (std::size_t d = 0; d < axes.size(); ++d)
      out.v[d].push_back(row[d]);
  });
  return out;
}

void sync_series(std::vector<TraceSeries> &series,
                 const std::vector<std::string> &sources,
                 const data::Store &store, std::size_t dims,
                 std::set<std::string> *seen) {
  const auto is_source = [&](const std::string &k) {
    return std::find(sources.begin(), sources.end(), k) != sources.end();
  };
  for (auto &s : series) {
    s.axes.resize(dims);
    for (auto &a : s.axes)
      if (!a.key.empty() && !is_source(a.key))
        a = {};
  }
  std::erase_if(series, [](const TraceSeries &s) {
    return std::all_of(s.axes.begin(), s.axes.end(),
                       [](const AxisSource &a) { return a.key.empty(); });
  });
  const auto used = [&](const std::string &k) {
    return std::any_of(series.begin(), series.end(), [&](const auto &s) {
      return std::any_of(s.axes.begin(), s.axes.end(),
                         [&](const auto &a) { return a.key == k; });
    });
  };
  if (seen) // forget removed sources, so adding one back assigns it again
    std::erase_if(*seen, [&](const std::string &k) { return !is_source(k); });
  for (const auto &key : sources) {
    const Series *s = find(store, key);
    if (!s || used(key) || (seen && seen->count(key)))
      continue;
    if (seen)
      seen->insert(key);
    if (s->kind == Kind::Array && !s->snaps.empty() &&
        s->snaps.back().second.size() >= dims) {
      TraceSeries ts;
      for (std::size_t d = 0; d < dims; ++d)
        ts.axes.push_back({key, (int)d});
      series.push_back(ts);
    } else if (s->kind == Kind::Number) {
      if (!series.empty()) {
        auto &axes = series.back().axes;
        auto free = std::find_if(axes.begin(), axes.end(),
                                 [](const auto &a) { return a.key.empty(); });
        if (free != axes.end()) {
          *free = {key, -1};
          continue;
        }
      }
      TraceSeries ts;
      ts.axes.resize(dims);
      ts.axes[0] = {key, -1};
      series.push_back(ts);
    }
  }
}

std::set<std::string> array_sources(const data::Store &store,
                                    const std::vector<std::string> &sources) {
  std::set<std::string> out;
  for (const auto &k : sources)
    if (const Series *s = find(store, k); s && s->kind == Kind::Array)
      out.insert(k);
  return out;
}

std::vector<ImU32> fade_colors(ImVec4 col, int n, bool fade) {
  std::vector<ImU32> colors(n);
  for (int i = 0; i < n; ++i) {
    col.w = fade ? 0.1f + 0.9f * float(i + 1) / float(n) : 1.0f;
    colors[i] = ImGui::GetColorU32(col);
  }
  return colors;
}

nlohmann::json series_to_json(const std::vector<TraceSeries> &series) {
  auto out = nlohmann::json::array();
  for (const auto &s : series) {
    auto axes = nlohmann::json::array();
    for (const auto &a : s.axes)
      axes.push_back({{"key", a.key}, {"index", a.index}});
    out.push_back(axes);
  }
  return out;
}

void series_from_json(const nlohmann::json &j, std::vector<TraceSeries> &series,
                      std::size_t dims) {
  if (!j.is_array())
    return;
  series.clear();
  for (const auto &js : j) {
    if (!js.is_array())
      continue;
    TraceSeries s;
    s.axes.resize(dims);
    for (std::size_t d = 0; d < dims && d < js.size(); ++d) {
      const auto &ja = js[d];
      if (!ja.is_object())
        continue;
      if (auto it = ja.find("key"); it != ja.end() && it->is_string())
        s.axes[d].key = it->get<std::string>();
      if (auto it = ja.find("index"); it != ja.end() && it->is_number_integer())
        s.axes[d].index = std::max(-1, it->get<int>());
    }
    series.push_back(s);
  }
}

void draw_series_options(std::vector<TraceSeries> &series,
                         const std::vector<std::string> &sources,
                         const std::set<std::string> &arrays) {
  static const char *names[] = {"X", "Y", "Z"};
  int remove = -1;
  for (int i = 0; i < (int)series.size(); ++i) {
    ImGui::PushID(i);
    ImGui::SeparatorText(("Series " + std::to_string(i + 1)).c_str());
    for (std::size_t d = 0; d < series[i].axes.size(); ++d) {
      AxisSource &a = series[i].axes[d];
      ImGui::PushID((int)d);
      ImGui::SetNextItemWidth(220);
      if (ImGui::BeginCombo(names[d],
                            a.key.empty() ? "(none)" : a.key.c_str())) {
        if (ImGui::Selectable("(none)", a.key.empty()))
          a = {};
        for (const auto &k : sources)
          if (ImGui::Selectable(k.c_str(), k == a.key)) {
            a.key = k;
            a.index = arrays.count(k) ? std::max(a.index, 0) : -1;
          }
        ImGui::EndCombo();
      }
      if (arrays.count(a.key)) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##index", &a.index);
        a.index = std::max(a.index, 0);
      }
      ImGui::PopID();
    }
    if (ImGui::MenuItem("Remove series"))
      remove = i;
    ImGui::PopID();
  }
  if (remove >= 0)
    series.erase(series.begin() + remove);
}

void XYView::draw_options() {
  draw_series_options(_series, _sources, _arrays);
  ImGui::Separator();
  ImGui::Checkbox("fade older", &_fade);
  ImGui::Checkbox("markers", &_markers);
}

void XYView::draw(const data::Store &store, const Timeline &tl) {
  sync_series(_series, _sources, store, 2, &_seen);
  _arrays = array_sources(store, _sources);

  struct Drawn {
    int series;
    TracePoints pts;
  };
  std::vector<Drawn> drawn;
  double xmin = INFINITY, xmax = -INFINITY, ymin = INFINITY, ymax = -INFINITY;
  for (int i = 0; i < (int)_series.size(); ++i) {
    if (!_series[i].complete())
      continue;
    TracePoints p = trace_points(store, _series[i].axes, tl.begin(), tl.now);
    if (p.size() == 0)
      continue;
    const auto [x0, x1] = std::minmax_element(p.v[0].begin(), p.v[0].end());
    const auto [y0, y1] = std::minmax_element(p.v[1].begin(), p.v[1].end());
    xmin = std::min(xmin, *x0);
    xmax = std::max(xmax, *x1);
    ymin = std::min(ymin, *y0);
    ymax = std::max(ymax, *y1);
    drawn.push_back({i, std::move(p)});
  }

  _scale.begin_frame();
  if (_scale.on()) {
    _scale.x.update(xmin, xmax);
    _scale.y.update(ymin, ymax);
  }
  if (!ImPlot::BeginPlot("##xy", ImVec2(-1, -1)))
    return;
  ImPlot::SetupAxes("x", "y");
  if (_scale.on() && _scale.x.valid) {
    ImPlot::SetupAxisLimits(ImAxis_X1, _scale.x.lo, _scale.x.hi,
                            ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, _scale.y.lo, _scale.y.hi,
                            ImPlotCond_Always);
  }
  for (const auto &[i, p] : drawn) {
    const int n = (int)p.size();
    auto colors = fade_colors(ImPlot::GetColormapColor(i), n, _fade);
    ImPlotSpec spec;
    spec.LineColors = colors.data();
    if (_markers)
      spec.Marker = ImPlotMarker_Circle;
    const auto label = "series " + std::to_string(i + 1);
    ImPlot::PlotLine(label.c_str(), p.v[0].data(), p.v[1].data(), n, spec);
  }
  if (drawn.empty())
    ImPlot::PlotText("XY: pick X and Y per series in Options", 0.5, 0.5);
  _scale.set_hovered(ImPlot::IsPlotHovered() ||
                     ImPlot::IsAxisHovered(ImAxis_X1) ||
                     ImPlot::IsAxisHovered(ImAxis_Y1));
  ImPlot::EndPlot();
}

nlohmann::json XYView::options() const {
  return {{"series", series_to_json(_series)},
          {"fade", _fade},
          {"markers", _markers}};
}

void XYView::set_options(const nlohmann::json &j) {
  if (auto it = j.find("series"); it != j.end())
    series_from_json(*it, _series, 2);
  if (auto it = j.find("fade"); it != j.end() && it->is_boolean())
    _fade = it->get<bool>();
  if (auto it = j.find("markers"); it != j.end() && it->is_boolean())
    _markers = it->get<bool>();
}

} // namespace mads_dash::ui
