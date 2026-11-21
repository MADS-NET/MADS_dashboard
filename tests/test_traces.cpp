#include <doctest/doctest.h>

#include "ui/views/xy_view.hpp"

using namespace mads_dash;
using nlohmann::json;
using ui::AxisSource;
using ui::TraceSeries;

namespace {
/// One message of topic "ts" whose time is `t` (timecode, recv_time 0).
void feed(data::Store &st, double t, json fields) {
  fields["timecode"] = t;
  st.ingest({"ts", fields, 0});
}

const std::vector<AxisSource> xy_axes = {{"/ts/p", 0}, {"/ts/p", 1}};

std::vector<std::vector<AxisSource>>
axes_of(const std::vector<TraceSeries> &s) {
  std::vector<std::vector<AxisSource>> out;
  for (const auto &t : s)
    out.push_back(t.axes);
  return out;
}
} // namespace

TEST_CASE("trace_points: one array gives its elements per snapshot") {
  data::Store st;
  for (int i = 1; i <= 4; ++i)
    feed(st, i, {{"p", {i, 10 * i, 100 * i}}});
  auto p = ui::trace_points(st, xy_axes, 2, 3); // inclusive window
  CHECK(p.v[0] == std::vector<double>{2, 3});
  CHECK(p.v[1] == std::vector<double>{20, 30});
  auto z = ui::trace_points(st, {{"/ts/p", 2}, {"/ts/p", 0}}, 0, 10);
  CHECK(z.v[0] == std::vector<double>{100, 200, 300, 400});
  CHECK(z.v[1] == std::vector<double>{1, 2, 3, 4});
  // an element that doesn't exist yields no points
  CHECK(ui::trace_points(st, {{"/ts/p", 0}, {"/ts/p", 7}}, 0, 10).size() == 0);
}

TEST_CASE("trace_points: numbers need the same time on every axis") {
  data::Store st;
  feed(st, 1, {{"a", 1}, {"b", 10}, {"c", 100}});
  feed(st, 2, {{"a", 2}, {"b", 20}}); // no c
  feed(st, 3, {{"a", 3}, {"b", 30}, {"c", 300}});
  feed(st, 4, {{"b", 40}, {"c", 400}}); // no a: not a time of the X axis
  const std::vector<AxisSource> axes = {{"/ts/a"}, {"/ts/b"}, {"/ts/c"}};
  auto p = ui::trace_points(st, axes, 0, 10);
  CHECK(p.v[0] == std::vector<double>{1, 3});
  CHECK(p.v[1] == std::vector<double>{10, 30});
  CHECK(p.v[2] == std::vector<double>{100, 300});
  CHECK(ui::trace_points(st, axes, 2, 10).v[0] == std::vector<double>{3});
  // the first axis sets the time base
  CHECK(ui::trace_points(st, {{"/ts/b"}, {"/ts/a"}}, 0, 10).size() == 3);
}

TEST_CASE("trace_points: incomplete or missing axes give nothing") {
  data::Store st;
  feed(st, 1, {{"a", 1}, {"p", {1, 2}}});
  CHECK(ui::trace_points(st, {{"/ts/a"}, {""}}, 0, 10).size() == 0);
  CHECK(ui::trace_points(st, {{"/ts/a"}, {"/ts/gone"}}, 0, 10).size() == 0);
  CHECK(ui::trace_points(st, {}, 0, 10).size() == 0);
  CHECK(TraceSeries{{{"/ts/a"}, {""}}}.complete() == false);
  CHECK(TraceSeries{{{"/ts/a"}, {"/ts/b"}}}.complete());
  // an Array axis without an index is unusable
  CHECK(ui::trace_points(st, {{"/ts/p"}, {"/ts/a"}}, 0, 10).size() == 0);
}

TEST_CASE("sync_series assigns arrays and numbers to series") {
  data::Store st;
  feed(st, 1,
       {{"p", {1, 2, 3}},
        {"q", {1, 2}},
        {"short", {1}},
        {"a", 1},
        {"b", 2},
        {"c", 3}});
  std::vector<TraceSeries> s;

  // an array with enough elements becomes a series of its first elements
  ui::sync_series(s, {"/ts/p"}, st, 3);
  CHECK(axes_of(s) == std::vector<std::vector<AxisSource>>{
                          {{"/ts/p", 0}, {"/ts/p", 1}, {"/ts/p", 2}}});
  // too short for XYZ, long enough for XY; "short" is never assigned
  ui::sync_series(s, {"/ts/p", "/ts/q", "/ts/short"}, st, 3);
  CHECK(s.size() == 1);
  std::vector<TraceSeries> xy;
  ui::sync_series(xy, {"/ts/q", "/ts/short"}, st, 2);
  CHECK(axes_of(xy) ==
        std::vector<std::vector<AxisSource>>{{{"/ts/q", 0}, {"/ts/q", 1}}});

  // numbers fill the next unset axis of the last series, then start a new one
  std::vector<TraceSeries> n;
  ui::sync_series(n, {"/ts/a", "/ts/b", "/ts/c"}, st, 2);
  CHECK(axes_of(n) == std::vector<std::vector<AxisSource>>{
                          {{"/ts/a", -1}, {"/ts/b", -1}}, {{"/ts/c", -1}, {}}});
  CHECK(!n[1].complete());

  // already used sources are left alone, new ones are added
  ui::sync_series(n, {"/ts/a", "/ts/b", "/ts/c", "/ts/p"}, st, 2);
  CHECK(n.size() == 3);
  CHECK(n[2].axes[1].index == 1);
}

TEST_CASE("sync_series unsets removed sources and drops empty series") {
  data::Store st;
  feed(st, 1, {{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}});
  std::vector<TraceSeries> s;
  ui::sync_series(s, {"/ts/a", "/ts/b", "/ts/c", "/ts/d"}, st, 2);
  REQUIRE(s.size() == 2);
  ui::sync_series(s, {"/ts/a", "/ts/c", "/ts/d"}, st, 2); // b removed
  REQUIRE(s.size() == 2);
  // b's slot is free again and refilled by nothing: the series is incomplete
  CHECK(s[0].axes[1].key.empty());
  CHECK(!s[0].complete());
  ui::sync_series(s, {"/ts/c", "/ts/d"}, st, 2); // a removed: series 1 is empty
  REQUIRE(s.size() == 1);
  CHECK(s[0].axes[0].key == "/ts/c");
  CHECK(s[0].axes[1].key == "/ts/d");
}

TEST_CASE("series options round trip, ignoring bad entries") {
  std::vector<TraceSeries> s = {{{{"/ts/p", 1}, {"/ts/q", -1}}}};
  json j = ui::series_to_json(s);
  std::vector<TraceSeries> back;
  ui::series_from_json(j, back, 2);
  CHECK(back[0].axes == s[0].axes);
  ui::series_from_json(json{7, json::array({1, {{"key", "k"}}})}, back, 3);
  REQUIRE(back.size() == 1);
  CHECK(back[0].axes.size() == 3);
  CHECK(back[0].axes[1].key == "k");
}
