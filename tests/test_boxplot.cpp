#include <doctest/doctest.h>

#include "ui/views/boxplot_view.hpp"

#include <cmath>

using mads_dash::ui::box_stats;

TEST_CASE("box_stats with an outlier") {
  // sorted 1..9 plus 100: q1=3.25 median=5.5 q3=7.75 iqr=4.5 fence=14.5
  const auto b = box_stats({5, 1, 2, 3, 4, 6, 7, 8, 9, 100});
  CHECK(b.q1 == doctest::Approx(3.25));
  CHECK(b.median == doctest::Approx(5.5));
  CHECK(b.q3 == doctest::Approx(7.75));
  CHECK(b.lo == doctest::Approx(1));
  CHECK(b.hi == doctest::Approx(9));
  REQUIRE(b.outliers.size() == 1);
  CHECK(b.outliers[0] == doctest::Approx(100));
}

TEST_CASE("box_stats single value") {
  const auto b = box_stats({4});
  CHECK(b.q1 == 4);
  CHECK(b.median == 4);
  CHECK(b.q3 == 4);
  CHECK(b.lo == 4);
  CHECK(b.hi == 4);
  CHECK(b.outliers.empty());
}

TEST_CASE("box_stats empty gives NaNs") {
  const auto b = box_stats({});
  CHECK(std::isnan(b.q1));
  CHECK(std::isnan(b.median));
  CHECK(std::isnan(b.q3));
  CHECK(std::isnan(b.lo));
  CHECK(std::isnan(b.hi));
  CHECK(b.outliers.empty());
}
