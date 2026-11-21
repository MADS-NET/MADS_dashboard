#include "ui/auto_scale.hpp"

#include <imgui.h>

#include <cmath>

namespace mads_dash::ui {

bool StickyRange::update(double dmin, double dmax) {
  if (!std::isfinite(dmin) || !std::isfinite(dmax) || dmin > dmax)
    return false;
  if (valid && dmin >= lo && dmax <= hi)
    return false;
  double half = (dmax - dmin) / 2;
  if (half == 0)
    half = dmin != 0 ? std::abs(dmin) : 1;
  lo = dmin - half;
  hi = dmax + half;
  valid = true;
  return true;
}

void AutoScale::set_on(bool on) {
  _on = on;
  if (on) {
    x.reset();
    y.reset();
    z.reset();
  }
}

void AutoScale::begin_frame(bool right_drag_ok) {
  if (!_hovered)
    return;
  const ImGuiIO &io = ImGui::GetIO();
  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    set_on(true);
  else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
           (!right_drag_ok && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) ||
           io.MouseWheel != 0)
    _on = false;
}

} // namespace mads_dash::ui
