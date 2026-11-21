#pragma once

#include "ui/auto_scale.hpp"
#include "ui/view.hpp"

#include <imgui.h>

#include <set>

namespace mads_dash::ui {

/// One axis of a trace: a keypath and, for Array keypaths, the element.
struct AxisSource {
  std::string key; ///< empty = unset
  int index = -1;  ///< array element; -1 (ignored) for Number keypaths
  bool operator==(const AxisSource &) const = default;
};

/// One trace: one AxisSource per dimension (X, Y[, Z]).
struct TraceSeries {
  std::vector<AxisSource> axes;
  bool complete() const;
};

/// Points of a trace: `v[axis][i]`, oldest first.
struct TracePoints {
  std::vector<std::vector<double>> v;
  std::size_t size() const { return v.empty() ? 0 : v[0].size(); }
};

/// The times in [t0, t1] of the first axis at which every axis has a value
/// with exactly that time (Array: a snapshot of that time with element
/// `index`; Number: a sample of that time).
TracePoints trace_points(const data::Store &store,
                         const std::vector<AxisSource> &axes, double t0,
                         double t1);

/// Keeps `series` (of `dims` axes each) in step with `sources`: axes whose key
/// is no longer a source become unset, empty series are dropped, and each
/// source not used by any series is assigned: an Array with at least `dims`
/// elements becomes a new series (key,0),(key,1)...; a Number fills the next
/// unset axis of the last series, else starts a new series as its X.
/// With `seen`, only sources never seen before are assigned, so a source the
/// user moved off an axis in Options is not turned into a new series.
void sync_series(std::vector<TraceSeries> &series,
                 const std::vector<std::string> &sources,
                 const data::Store &store, std::size_t dims,
                 std::set<std::string> *seen = nullptr);

/// The members of `sources` that are Array keypaths.
std::set<std::string> array_sources(const data::Store &store,
                                    const std::vector<std::string> &sources);

/// Opacity ramp for per-point colours: oldest faintest when `fade`.
std::vector<ImU32> fade_colors(ImVec4 col, int n, bool fade);

/// "series" option: keys and indices, and back (tolerant of bad input).
nlohmann::json series_to_json(const std::vector<TraceSeries> &series);
void series_from_json(const nlohmann::json &j, std::vector<TraceSeries> &series,
                      std::size_t dims);

/// Per-series combos and index inputs of the Options menu. `arrays` are the
/// sources that are Arrays (they get an index input).
void draw_series_options(std::vector<TraceSeries> &series,
                         const std::vector<std::string> &sources,
                         const std::set<std::string> &arrays);

/// XY plot with several traces (PLAN §4.3): each series draws (x(t), y(t)) over
/// the timeline's window, older points fading out when `fade` is on.
class XYView : public View {
public:
  using View::View;
  const char *type_name() const override { return "XY"; }
  bool accepts(data::Kind k) const override {
    return k == data::Kind::Number || k == data::Kind::Array;
  }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;
  nlohmann::json options() const override;
  void set_options(const nlohmann::json &j) override;

private:
  std::vector<TraceSeries> _series;
  std::set<std::string> _arrays; ///< sources that are Arrays, as of draw()
  std::set<std::string> _seen;   ///< sources already given to a series
  AutoScale _scale;
  bool _fade = true;
  bool _markers = false;
};

} // namespace mads_dash::ui
