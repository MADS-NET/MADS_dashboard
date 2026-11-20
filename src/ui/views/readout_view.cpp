#include "ui/views/readout_view.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace mads_dash::ui {

namespace {
using data::Kind;
using data::Series;

std::string num(double v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.6g", v);
  return buf;
}

/// Time of the latest sample at or before `now`, or false if none.
bool latest_time(const Series &s, double now, double &t) {
  const auto upto = [&](const auto &c, auto time) {
    return std::upper_bound(
               c.begin(), c.end(), now,
               [&](double n, const auto &e) { return n < time(e); }) -
           c.begin();
  };
  std::ptrdiff_t i = 0;
  switch (s.kind) {
  case Kind::Number:
  case Kind::Bool:
    i = upto(s.t, [](double e) { return e; });
    t = i ? s.t[i - 1] : 0;
    break;
  case Kind::String:
    i = upto(s.events, [](const auto &e) { return e.first; });
    t = i ? s.events[i - 1].first : 0;
    break;
  case Kind::Array:
    i = upto(s.snaps, [](const auto &e) { return e.first; });
    t = i ? s.snaps[i - 1].first : 0;
    break;
  default:
    break;
  }
  return i > 0;
}
} // namespace

std::string readout_text(const Series &s, double now) {
  double t = 0;
  if (!latest_time(s, now, t))
    return "—";
  switch (s.kind) {
  case Kind::Number: {
    const auto i = std::upper_bound(s.t.begin(), s.t.end(), now) - s.t.begin();
    return num(s.v[i - 1]);
  }
  case Kind::Bool: {
    const auto i = std::upper_bound(s.t.begin(), s.t.end(), now) - s.t.begin();
    return s.v[i - 1] != 0 ? "true" : "false";
  }
  case Kind::String: {
    const auto it =
        std::upper_bound(s.events.begin(), s.events.end(), now,
                         [](double n, const auto &e) { return n < e.first; });
    return (it - 1)->second;
  }
  case Kind::Array: {
    const auto it =
        std::upper_bound(s.snaps.begin(), s.snaps.end(), now,
                         [](double n, const auto &e) { return n < e.first; });
    const auto &a = (it - 1)->second;
    std::string out = "[";
    for (std::size_t k = 0; k < a.size() && k < 8; ++k)
      out += (k ? ", " : "") + num(a[k]);
    return out + (a.size() > 8 ? ", …]" : "]");
  }
  default:
    return "—";
  }
}

void ReadoutView::draw_options() { ImGui::Checkbox("big values", &_big); }

void ReadoutView::draw(const data::Store &store, const Timeline &tl) {
  if (!ImGui::BeginTable("##readout", 3,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_SizingStretchProp))
    return;
  ImGui::TableSetupColumn("keypath");
  ImGui::TableSetupColumn("value");
  ImGui::TableSetupColumn("age (s)");
  ImGui::TableHeadersRow();
  for (const auto &key : _sources) {
    const auto it = store.series().find(key);
    if (it == store.series().end())
      continue;
    double t = 0;
    const bool has = latest_time(it->second, tl.now, t);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(key.c_str());
    ImGui::TableNextColumn();
    if (_big)
      ImGui::SetWindowFontScale(2);
    ImGui::TextUnformatted(readout_text(it->second, tl.now).c_str());
    if (_big)
      ImGui::SetWindowFontScale(1);
    ImGui::TableNextColumn();
    if (has)
      ImGui::Text("%.3f", tl.now - t);
    else
      ImGui::TextUnformatted("—");
  }
  ImGui::EndTable();
}

} // namespace mads_dash::ui
