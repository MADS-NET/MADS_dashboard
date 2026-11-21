#pragma once

namespace mads_dash::ui {

/// Sticky range of one axis: when the data leave it, it grows to the data
/// range plus half of it on each side (data [0, 10] -> axis [-5, 15]), then
/// stays put until the data leave it again. Avoids an axis that jitters with
/// every new sample.
struct StickyRange {
  bool valid = false;
  double lo = 0, hi = 1;
  /// Returns true if the range changed. A zero-width range widens by |value|
  /// (or 1 for 0) on each side.
  bool update(double dmin, double dmax);
  void reset() { valid = false; }
};

/// Autoscale state of one plot: on by default; a double-click on the plot
/// engages it (and refits), dragging, box-zooming or scrolling releases it.
class AutoScale {
public:
  bool on() const { return _on; }
  void set_on(bool on);
  /// Call once per frame before BeginPlot: applies last frame's mouse input
  /// on the plot. With `right_drag_ok` (3D: right-drag rotates) a right drag
  /// does not release the autoscale.
  void begin_frame(bool right_drag_ok = false);
  /// Call inside the plot, after its setup, with whether it's hovered.
  void set_hovered(bool hovered) { _hovered = hovered; }

  StickyRange x, y, z;

private:
  bool _on = true;
  bool _hovered = false;
};

} // namespace mads_dash::ui
