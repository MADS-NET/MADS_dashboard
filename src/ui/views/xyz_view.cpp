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

namespace {
using data::Kind;
using data::Series;

const Series *find(const data::Store &store, const std::string &key) {
  auto it = store.series().find(key);
  return it == store.series().end() ? nullptr : &it->second;
}
} // namespace

void XYZView::draw_options() {
  ImGui::SetNextItemWidth(150);
  ImGui::SliderInt("history", &_history, 1, 5000, "%d",
                   ImGuiSliderFlags_Logarithmic);
  ImGui::Checkbox("fade older", &_fade);
  ImGui::Checkbox("markers", &_markers);
  ImGui::Checkbox("pose axes", &_pose_axes);
}

nlohmann::json XYZView::options() const {
  return {{"history", _history},
          {"fade", _fade},
          {"markers", _markers},
          {"pose_axes", _pose_axes}};
}

void XYZView::set_options(const nlohmann::json &j) {
  if (auto it = j.find("history"); it != j.end() && it->is_number_integer())
    _history = std::clamp(it->get<int>(), 1, 5000);
  for (auto [key, flag] :
       {std::pair{"fade", &_fade}, std::pair{"markers", &_markers},
        std::pair{"pose_axes", &_pose_axes}})
    if (auto it = j.find(key); it != j.end() && it->is_boolean())
      *flag = it->get<bool>();
}

void XYZView::draw(const data::Store &store, const Timeline &tl) {
  std::vector<double> xs, ys, zs;
  std::vector<double> pose; // latest [x y z qw qx qy qz], if any
  const Series *a = _sources.size() == 1 ? find(store, _sources[0]) : nullptr;
  if (a && a->kind == Kind::Array) {
    const std::size_t end =
        std::upper_bound(a->snaps.begin(), a->snaps.end(), tl.now,
                         [](double n, const auto &s) { return n < s.first; }) -
        a->snaps.begin();
    const std::size_t begin = end > (std::size_t)_history ? end - _history : 0;
    for (std::size_t i = begin; i < end; ++i) {
      const auto &v = a->snaps[i].second;
      if (v.size() < 3)
        continue;
      xs.push_back(v[0]);
      ys.push_back(v[1]);
      zs.push_back(v[2]);
      if (i + 1 == end && v.size() >= 7)
        pose = v;
    }
  } else if (_sources.size() == 3) {
    const Series *sx = find(store, _sources[0]), *sy = find(store, _sources[1]),
                 *sz = find(store, _sources[2]);
    if (sx && sy && sz && sx->kind == Kind::Number &&
        sy->kind == Kind::Number && sz->kind == Kind::Number) {
      const std::size_t end =
          std::upper_bound(sx->t.begin(), sx->t.end(), tl.now) - sx->t.begin();
      const std::size_t begin =
          end > (std::size_t)_history ? end - _history : 0;
      auto at = [](const Series *s, double t, double &v) {
        auto it = std::lower_bound(s->t.begin(), s->t.end(), t);
        if (it == s->t.end() || *it != t)
          return false;
        v = s->v[it - s->t.begin()];
        return true;
      };
      for (std::size_t i = begin; i < end; ++i) {
        double y, z;
        if (at(sy, sx->t[i], y) && at(sz, sx->t[i], z)) {
          xs.push_back(sx->v[i]);
          ys.push_back(y);
          zs.push_back(z);
        }
      }
    }
  }

  if (!ImPlot3D::BeginPlot("##xyz", ImVec2(-1, -1)))
    return;
  const ImPlot3DAxisFlags fit = tl.paused ? 0 : ImPlot3DAxisFlags_AutoFit;
  ImPlot3D::SetupAxes("x", "y", "z", fit, fit, fit);
  if (xs.empty()) {
    ImPlot3D::PlotText("XYZ needs one array [x, y, z, ...] or three numbers", 0,
                       0, 0);
    ImPlot3D::EndPlot();
    return;
  }
  const int n = (int)xs.size();
  const ImVec4 col = ImPlot3D::GetColormapColor(0);
  std::vector<ImU32> colors(n);
  for (int i = 0; i < n; ++i) {
    ImVec4 c = col;
    c.w = _fade ? 0.1f + 0.9f * float(i + 1) / float(n) : 1.0f;
    colors[i] = ImGui::GetColorU32(c);
  }
  ImPlot3DSpec spec;
  spec.LineColors = colors.data();
  spec.Marker = _markers ? ImPlot3DMarker_Circle : ImPlot3DMarker_None;
  ImPlot3D::PlotLine("trace", xs.data(), ys.data(), zs.data(), n, spec);

  if (_pose_axes && !pose.empty()) {
    // axis length: 10% of the trace's largest extent
    auto extent = [](const std::vector<double> &v) {
      auto [lo, hi] = std::minmax_element(v.begin(), v.end());
      return *hi - *lo;
    };
    double len = 0.1 * std::max({extent(xs), extent(ys), extent(zs)});
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
      ImPlot3D::PlotLine(axis_name[k], px, py, pz, 2, as);
    }
  }
  ImPlot3D::EndPlot();
}

} // namespace mads_dash::ui
