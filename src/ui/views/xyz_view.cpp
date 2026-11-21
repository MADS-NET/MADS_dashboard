#include "ui/views/xyz_view.hpp"

#include <implot3d.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace mads_dash::ui {

std::array<double, 3> rotate(std::array<double, 4> q, std::array<double, 3> v) {
  const double n =
      std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (n == 0)
    return v;
  const double w = q[0] / n, x = q[1] / n, y = q[2] / n, z = q[3] / n;
  // v' = v + 2w (u x v) + 2 u x (u x v), with u = (x, y, z)
  const std::array<double, 3> c{y * v[2] - z * v[1], z * v[0] - x * v[2],
                                x * v[1] - y * v[0]};
  const std::array<double, 3> cc{y * c[2] - z * c[1], z * c[0] - x * c[2],
                                 x * c[1] - y * c[0]};
  return {v[0] + 2 * (w * c[0] + cc[0]), v[1] + 2 * (w * c[1] + cc[1]),
          v[2] + 2 * (w * c[2] + cc[2])};
}

void XYZView::draw_options() {
  draw_series_options(_series, _sources, _arrays);
  ImGui::Separator();
  ImGui::Checkbox("fade older", &_fade);
  ImGui::Checkbox("markers", &_markers);
  ImGui::Checkbox("pose axes", &_pose_axes);
  ImGui::Checkbox("equal scale", &_equal);
}

nlohmann::json XYZView::options() const {
  return {{"series", series_to_json(_series)},
          {"fade", _fade},
          {"markers", _markers},
          {"pose_axes", _pose_axes},
          {"equal", _equal}};
}

void XYZView::set_options(const nlohmann::json &j) {
  if (auto it = j.find("series"); it != j.end())
    series_from_json(*it, _series, 3);
  for (auto [key, flag] :
       {std::pair{"fade", &_fade}, std::pair{"markers", &_markers},
        std::pair{"pose_axes", &_pose_axes}, std::pair{"equal", &_equal}})
    if (auto it = j.find(key); it != j.end() && it->is_boolean())
      *flag = it->get<bool>();
}

namespace {
/// Latest [x y z qw qx qy qz] of an Array keypath at or before `now`, if any.
std::vector<double> latest_pose(const data::Store &store,
                                const std::string &key, double now) {
  auto it = store.series().find(key);
  if (it == store.series().end() || it->second.kind != data::Kind::Array)
    return {};
  const auto &snaps = it->second.snaps;
  auto end =
      std::partition_point(snaps.begin(), snaps.end(),
                           [&](const auto &s) { return s.first <= now; });
  if (end == snaps.begin() || (end - 1)->second.size() != 7)
    return {};
  return (end - 1)->second;
}

double extent(const std::vector<double> &v) {
  auto [lo, hi] = std::minmax_element(v.begin(), v.end());
  return *hi - *lo;
}
} // namespace

void XYZView::draw(const data::Store &store, const Timeline &tl) {
  sync_series(_series, _sources, store, 3);
  _arrays = array_sources(store, _sources);

  struct Drawn {
    int series;
    TracePoints pts;
  };
  std::vector<Drawn> drawn;
  double lo[3] = {INFINITY, INFINITY, INFINITY};
  double hi[3] = {-INFINITY, -INFINITY, -INFINITY};
  for (int i = 0; i < (int)_series.size(); ++i) {
    if (!_series[i].complete())
      continue;
    TracePoints p = trace_points(store, _series[i].axes, tl.begin(), tl.now);
    if (p.size() == 0)
      continue;
    for (int d = 0; d < 3; ++d) {
      const auto [a, b] = std::minmax_element(p.v[d].begin(), p.v[d].end());
      lo[d] = std::min(lo[d], *a);
      hi[d] = std::max(hi[d], *b);
    }
    drawn.push_back({i, std::move(p)});
  }

  _scale.begin_frame(true);
  if (_scale.on()) {
    _scale.x.update(lo[0], hi[0]);
    _scale.y.update(lo[1], hi[1]);
    _scale.z.update(lo[2], hi[2]);
  }
  if (!ImPlot3D::BeginPlot("##xyz", ImVec2(-1, -1),
                           _equal ? ImPlot3DFlags_Equal : 0))
    return;
  ImPlot3D::SetupAxes("x", "y", "z");
  if (_scale.on() && _scale.x.valid) {
    StickyRange *r[3] = {&_scale.x, &_scale.y, &_scale.z};
    double span = 0;
    for (auto *s : r)
      span = std::max(span, s->hi - s->lo);
    for (int d = 0; d < 3; ++d) {
      double a = r[d]->lo, b = r[d]->hi;
      if (_equal) { // one common span, centred per axis
        const double c = 0.5 * (a + b);
        a = c - 0.5 * span;
        b = c + 0.5 * span;
      }
      ImPlot3D::SetupAxisLimits(d, a, b, ImPlot3DCond_Always);
    }
  }
  for (const auto &[i, p] : drawn) {
    const int n = (int)p.size();
    auto colors = fade_colors(ImPlot3D::GetColormapColor(i), n, _fade);
    ImPlot3DSpec spec;
    spec.LineColors = colors.data();
    spec.Marker = _markers ? ImPlot3DMarker_Circle : ImPlot3DMarker_None;
    const auto label = "series " + std::to_string(i + 1);
    ImPlot3D::PlotLine(label.c_str(), p.v[0].data(), p.v[1].data(),
                       p.v[2].data(), n, spec);

    const auto pose = _pose_axes
                          ? latest_pose(store, _series[i].axes[0].key, tl.now)
                          : std::vector<double>();
    if (pose.empty())
      continue;
    // axis length: 10% of the trace's largest extent
    double len =
        0.1 * std::max({extent(p.v[0]), extent(p.v[1]), extent(p.v[2])});
    if (len <= 0)
      len = 1;
    const std::array<double, 4> q{pose[3], pose[4], pose[5], pose[6]};
    const ImVec4 axis_col[3] = {
        {1, 0.3f, 0.3f, 1}, {0.3f, 1, 0.3f, 1}, {0.4f, 0.6f, 1, 1}};
    const char *axis_name[3] = {"pose x", "pose y", "pose z"};
    for (int k = 0; k < 3; ++k) {
      std::array<double, 3> e{0, 0, 0};
      e[k] = len;
      const auto d = rotate(q, e);
      const double px[2] = {pose[0], pose[0] + d[0]},
                   py[2] = {pose[1], pose[1] + d[1]},
                   pz[2] = {pose[2], pose[2] + d[2]};
      ImPlot3DSpec as;
      as.LineColor = axis_col[k];
      as.LineWeight = 3;
      as.Marker = ImPlot3DMarker_None;
      as.Flags = ImPlot3DItemFlags_NoLegend;
      ImPlot3D::PlotLine(axis_name[k], px, py, pz, 2, as);
    }
  }
  if (drawn.empty())
    ImPlot3D::PlotText("XYZ: pick X, Y and Z per series in Options", 0, 0, 0);
  ImPlot3D::EndPlot();
  // ImPlot3D has no IsPlotHovered(): the view window stands in for it
  _scale.set_hovered(ImGui::IsWindowHovered());
}

} // namespace mads_dash::ui
