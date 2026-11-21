#include <doctest/doctest.h>

#include "data/store.hpp"

using namespace mads_dash::data;
using nlohmann::json;

TEST_CASE("flatten builds keypaths and classifies leaves") {
  json j = {{"a", {{"b", 1.5}}},
            {"s", "x"},
            {"ok", true},
            {"acc", {1, 2, 3}},
            {"map", {{1, 2}, {3, 4}}},
            {"objs", {{{"v", 7}}}}};
  std::map<std::string, Kind> got;
  flatten("/t", j,
          [&](const std::string &p, const json &v) { got[p] = kind_of(v); });
  CHECK(got.at("/t/a/b") == Kind::Number);
  CHECK(got.at("/t/s") == Kind::String);
  CHECK(got.at("/t/ok") == Kind::Bool);
  CHECK(got.at("/t/acc") == Kind::Array);
  CHECK(got.at("/t/acc[2]") == Kind::Number);
  CHECK(got.at("/t/map") == Kind::Matrix);
  CHECK(got.count("/t/map[0]") == 0);
  CHECK(got.at("/t/objs[0]/v") == Kind::Number);
}

TEST_CASE("store keeps bounded array snapshots") {
  Store st(100, 3);
  st.set_fixed_utc_offset(0);
  for (int i = 0; i < 5; ++i)
    st.ingest({"p", {{"timecode", 1.0 * i}, {"pos", {i, 2 * i}}}, 0});
  const auto &s = st.series().at("/p/pos");
  REQUIRE(s.snaps.size() == 3);
  CHECK(s.snaps.front().first == 2.0);
  CHECK(s.snaps.back().second == std::vector<double>{4, 8});
}

TEST_CASE("store uses timecode, skips time fields, trims, records events") {
  Store st(8);
  st.set_fixed_utc_offset(0);
  for (int i = 0; i < 20; ++i)
    st.ingest({"sig",
               {{"timecode", 100.0 + i},
                {"timestamp", "x"},
                {"x", i},
                {"state", i < 10 ? "idle" : "run"}},
               0});
  const auto &s = st.series();
  CHECK(s.count("/sig/timecode") == 1); // the time axis, shown greyed
  CHECK(s.count("/sig/timestamp") == 0);
  const auto &x = s.at("/sig/x");
  CHECK(x.t.size() <= 10);
  CHECK(x.t.back() == 119.0);
  CHECK(x.v.back() == 19.0);
  CHECK(s.at("/sig/state").events.size() == 2);
  CHECK(st.latest_time() == 119.0);
  CHECK(st.messages() == 20);
}

TEST_CASE("timecode to UTC, across midnight") {
  const double day = 86400, d = 20000 * day; // some midnight
  CHECK(timecode_to_utc(3600, d + 3700) == d + 3600);
  // sent at 23:59:59, received at 00:00:01 the next day
  CHECK(timecode_to_utc(day - 1, d + 1) == d - 1);
  // receiver clock slightly behind: received at 23:59:59, stamped 00:00:01
  CHECK(timecode_to_utc(1, d - 1) == d + 1);
}

TEST_CASE("ISO 8601 parsing") {
  CHECK(parse_iso8601("1970-01-01T00:00:00Z") == 0.0);
  CHECK(parse_iso8601("2024-06-01T12:34:56.5Z") == 1717245296.5);
  CHECK(parse_iso8601("2024-06-01T14:34:56.5+02:00") == 1717245296.5);
  CHECK(parse_iso8601("2024-06-01T10:34:56.5-0200") == 1717245296.5);
  CHECK(parse_iso8601("2024-06-01 12:34:56") == 1717245296.0);
  CHECK_FALSE(parse_iso8601("yesterday"));
}

TEST_CASE("per-topic time field: numeric and $date") {
  Store st;
  st.set_fixed_utc_offset(0);
  st.ingest({"a", {{"timecode", 10.0}, {"x", 1}, {"sim", 0.5}}, 0});
  REQUIRE(st.series().count("/a/sim"));
  CHECK(Store::can_be_time("/a/sim", st.series().at("/a/sim")));
  CHECK_FALSE(Store::can_be_time("/a/timestamp", Series{Kind::String}));

  st.set_time_field("a", "/a/sim");
  CHECK(st.series().empty()); // topic cleared
  st.ingest({"a", {{"timecode", 11.0}, {"x", 2}, {"sim", 0.75}}, 0});
  st.ingest({"a", {{"timecode", 12.0}, {"x", 3}}, 0}); // no time: skipped
  const auto &x = st.series().at("/a/x");
  CHECK(x.t == std::vector<double>{0.75});
  CHECK(st.series().count("/a/timecode")); // now an ordinary field
  CHECK(st.series().count("/a/sim") == 1); // the time axis is kept
  CHECK_FALSE(st.time_base("a").clock);    // small numbers: not a clock

  st.set_time_field("a", "/a/when/$date");
  st.ingest(
      {"a", {{"x", 4}, {"when", {{"$date", "1970-01-02T00:00:00Z"}}}}, 0});
  CHECK(st.series().at("/a/x").t == std::vector<double>{86400.0});
  CHECK(st.time_base("a").clock);

  st.set_time_field("a", "");
  CHECK(st.time_base("a").field.empty());
}

TEST_CASE("retention trims old samples; memory estimate") {
  Store st;
  st.set_fixed_utc_offset(0);
  for (int i = 0; i < 100; ++i)
    st.ingest({"r",
               {{"timecode", 1000.0 + i},
                {"x", i},
                {"s", i % 10 ? "a" : "b"},
                {"arr", {i, i}}},
               0});
  const std::size_t before = st.memory_bytes();
  CHECK(before > 100 * 2 * sizeof(double));
  st.set_retention(10);
  st.trim();
  const auto &x = st.series().at("/r/x");
  CHECK(x.t.front() >= 1099.0 - 10);
  CHECK(x.t.size() == 11);
  CHECK(st.series().at("/r/arr").snaps.size() == 11);
  CHECK(st.first_time() == 1089.0);
  st.set_retention(0); // unlimited: trim() keeps everything
  st.trim();
  CHECK(x.t.size() == 11);
}

TEST_CASE("MADS v3 timestamp {$date: ...} is a number and can be the time") {
  Store st;
  st.set_fixed_utc_offset(0);
  const json ts = {{"$date", "1970-01-02T01:00:00.500+0100"}};
  st.ingest({"a", {{"timecode", 5.0}, {"timestamp", ts}, {"x", 1}}, 0});
  const auto &d = st.series().at("/a/timestamp/$date");
  CHECK(d.kind == Kind::Number);
  CHECK(d.v == std::vector<double>{86400.5});
  CHECK(d.events.empty()); // not one string event per message
  CHECK(Store::can_be_time("/a/timestamp/$date", d));
  st.set_time_field("a", "/a/timestamp/$date");
  st.ingest({"a", {{"timecode", 6.0}, {"timestamp", ts}, {"x", 2}}, 0});
  CHECK(st.series().at("/a/x").t == std::vector<double>{86400.5});
  CHECK(st.series().count("/a/timestamp/$date") == 1); // kept as a field
  CHECK(st.time_base("a").clock);
}

TEST_CASE("timecode is local time of day; offset and day wrap") {
  const double d = 20000 * 86400.0;                 // a UTC midnight
  CHECK(timecode_to_utc(3600, d + 100, 3600) == d); // 01:00 local = 00:00 UTC
  CHECK(timecode_to_utc(0.5, d - 3600 + 1, 3600) == d - 3600 + 0.5); // local
                                                                     // midnight
  // local 23:30 at UTC+2 is 21:30 UTC of the same UTC day
  CHECK(timecode_to_utc(84600, d + 77400 + 5, 7200) == d + 77400);
  CHECK(std::abs(local_utc_offset(d)) <= 14 * 3600); // some real zone
}

TEST_CASE("switching the time field re-derives first and latest time") {
  Store st;
  st.set_fixed_utc_offset(0);
  st.ingest({"a", {{"timecode", 100.0}, {"x", 1}, {"sim", 1.0}}, 0});
  st.ingest({"b", {{"timecode", 50.0}, {"y", 1}}, 0});
  CHECK(st.latest_time() == 100.0);
  st.set_time_field("a", "/a/sim"); // topic a cleared: only b's data remain
  CHECK(st.latest_time() == 50.0);
  CHECK(st.first_time() == 50.0);
  st.ingest({"a", {{"timecode", 101.0}, {"x", 2}, {"sim", 2.0}}, 0});
  CHECK(st.first_time() == 2.0);
}

TEST_CASE("timecode and timestamp of one live MADS message agree") {
  // captured from testsource: timecode is local time of day (UTC+1 here)
  const double ts = *parse_iso8601("2026-11-21T10:17:57.399+0100");
  const double tc = timecode_to_utc(37077.36, ts + 0.002, 3600);
  CHECK(std::abs(tc - ts) < 0.04); // timecode is quantised to 1/25 s
}

TEST_CASE("is_time_field follows the time base") {
  Store st;
  st.set_fixed_utc_offset(0);
  st.ingest({"a", {{"timecode", 1.0}, {"sim", 2.0}}, 0});
  CHECK(st.is_time_field("/a/timecode"));
  CHECK_FALSE(st.is_time_field("/a/sim"));
  st.set_time_field("a", "/a/sim");
  CHECK(st.is_time_field("/a/sim"));
  CHECK_FALSE(st.is_time_field("/a/timecode"));
}
