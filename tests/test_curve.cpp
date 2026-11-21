#include <doctest/doctest.h>

#include "ui/views/curve_view.hpp"

using namespace mads_dash;

namespace {
data::Series
arr(std::initializer_list<std::pair<double, std::vector<double>>> l) {
  data::Series s;
  s.kind = data::Kind::Array;
  s.snaps.assign(l.begin(), l.end());
  return s;
}
void put(data::Store &st, const char *topic, const nlohmann::json &p,
         double t = 1000) {
  st.ingest({topic, p, t});
}
} // namespace

TEST_CASE("curve_points pairs snapshots of equal time") {
  const auto x = arr({{1, {1, 2}}, {2, {3, 4}}, {3, {5, 6, 7}}});
  const auto y = arr({{1, {10, 20}}, {2, {30, 40}}, {3.5, {50, 60}}});
  std::vector<double> xs, ys;

  SUBCASE("latest pair") {
    // t = 3 has no partner in y: the latest equal-time pair is t = 2
    REQUIRE(ui::curve_points(x, y, 10, xs, ys));
    CHECK(xs == std::vector<double>{3, 4});
    CHECK(ys == std::vector<double>{30, 40});
  }
  SUBCASE("now in the past") {
    REQUIRE(ui::curve_points(x, y, 1.5, xs, ys));
    CHECK(xs == std::vector<double>{1, 2});
    CHECK(ys == std::vector<double>{10, 20});
  }
  SUBCASE("now before the first sample") {
    CHECK_FALSE(ui::curve_points(x, y, 0.5, xs, ys));
  }
  SUBCASE("mismatched times") {
    const auto z = arr({{1.1, {1, 2}}, {2.1, {3, 4}}});
    CHECK_FALSE(ui::curve_points(x, z, 10, xs, ys));
  }
  SUBCASE("unequal lengths use the shorter") {
    const auto w = arr({{3, {1, 2}}});
    REQUIRE(ui::curve_points(x, w, 10, xs, ys));
    CHECK(xs.size() == 2);
    CHECK(ys.size() == 2);
  }
}

TEST_CASE("curve_pair") {
  std::string xk, yk;
  SUBCASE("the xy case") {
    data::Store st;
    put(st, "ts", {{"xy", {{"y", {1, 2, 3}}, {"x", {4, 5, 6}}}}});
    REQUIRE(ui::curve_pair(st, "/ts/xy", xk, yk));
    CHECK(xk == "/ts/xy/x");
    CHECK(yk == "/ts/xy/y");
  }
  SUBCASE("one array") {
    data::Store st;
    put(st, "ts", {{"xy", {{"x", {1, 2, 3}}}}});
    CHECK_FALSE(ui::curve_pair(st, "/ts/xy", xk, yk));
  }
  SUBCASE("three arrays") {
    data::Store st;
    put(st, "ts", {{"xy", {{"x", {1, 2}}, {"y", {1, 2}}, {"z", {1, 2}}}}});
    CHECK_FALSE(ui::curve_pair(st, "/ts/xy", xk, yk));
  }
  SUBCASE("unequal lengths") {
    data::Store st;
    put(st, "ts", {{"xy", {{"x", {1, 2}}, {"y", {1, 2, 3}}}}});
    CHECK_FALSE(ui::curve_pair(st, "/ts/xy", xk, yk));
  }
  SUBCASE("nested children are not counted") {
    data::Store st;
    put(st, "ts",
        {{"xy", {{"x", {1, 2}}, {"y", {1, 2}}, {"sub", {{"z", {1, 2}}}}}}});
    REQUIRE(ui::curve_pair(st, "/ts/xy", xk, yk));
    CHECK(yk == "/ts/xy/y");
    // only nested arrays: no direct children
    CHECK_FALSE(ui::curve_pair(st, "/ts", xk, yk));
  }
}
