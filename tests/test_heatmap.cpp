#include <doctest/doctest.h>

#include "ui/views/heatmap_view.hpp"

using namespace mads_dash::ui;
using nlohmann::json;

TEST_CASE("matrix_values flattens row-major") {
  std::vector<double> v;
  int r = 0, c = 0;
  CHECK(matrix_values(json::parse("[[1,2,3],[4,5,6]]"), v, r, c));
  CHECK(r == 2);
  CHECK(c == 3);
  CHECK(v == std::vector<double>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("matrix_values rejects bad input") {
  std::vector<double> v;
  int r = 0, c = 0;
  CHECK_FALSE(matrix_values(json::parse("[]"), v, r, c));
  CHECK_FALSE(matrix_values(json::parse("[[]]"), v, r, c));
  CHECK_FALSE(matrix_values(json::parse("[[1,2],[3]]"), v, r, c));
  CHECK_FALSE(matrix_values(json::parse("[1,2]"), v, r, c));
  CHECK_FALSE(matrix_values(json::parse("[[1,\"a\"]]"), v, r, c));
  CHECK_FALSE(matrix_values(json(), v, r, c));
}

TEST_CASE("value_range") {
  auto [lo, hi] = value_range({3, -1, 7});
  CHECK(lo == -1);
  CHECK(hi == 7);
  CHECK(value_range({}) == std::pair<double, double>(0, 1));
  auto [a, b] = value_range({2, 2});
  CHECK(a < b);
}
