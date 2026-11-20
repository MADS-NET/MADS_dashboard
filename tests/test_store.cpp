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
  for (int i = 0; i < 5; ++i)
    st.ingest({"p", {{"timecode", 1.0 * i}, {"pos", {i, 2 * i}}}, 0});
  const auto &s = st.series().at("/p/pos");
  REQUIRE(s.snaps.size() == 3);
  CHECK(s.snaps.front().first == 2.0);
  CHECK(s.snaps.back().second == std::vector<double>{4, 8});
}

TEST_CASE("store uses timecode, skips time fields, trims, records events") {
  Store st(8);
  for (int i = 0; i < 20; ++i)
    st.ingest({"sig",
               {{"timecode", 100.0 + i},
                {"timestamp", "x"},
                {"x", i},
                {"state", i < 10 ? "idle" : "run"}},
               0});
  const auto &s = st.series();
  CHECK(s.count("/sig/timecode") == 0);
  CHECK(s.count("/sig/timestamp") == 0);
  const auto &x = s.at("/sig/x");
  CHECK(x.t.size() <= 10);
  CHECK(x.t.back() == 119.0);
  CHECK(x.v.back() == 19.0);
  CHECK(s.at("/sig/state").events.size() == 2);
  CHECK(st.latest_time() == 119.0);
  CHECK(st.messages() == 20);
}
