#include <doctest/doctest.h>

#include "ui/views/xyz_view.hpp"

#include <cmath>

using mads_dash::ui::rotate;

TEST_CASE("quaternion rotation") {
  const double s = std::sqrt(0.5);
  auto approx = [](std::array<double, 3> a, std::array<double, 3> b) {
    for (int i = 0; i < 3; ++i)
      CHECK(a[i] == doctest::Approx(b[i]).epsilon(1e-12));
  };
  approx(rotate({1, 0, 0, 0}, {1, 2, 3}), {1, 2, 3}); // identity
  approx(rotate({s, 0, 0, s}, {1, 0, 0}), {0, 1, 0}); // 90 deg about z
  approx(rotate({s, s, 0, 0}, {0, 1, 0}), {0, 0, 1}); // 90 deg about x
  approx(rotate({2, 0, 0, 2}, {1, 0, 0}), {0, 1, 0}); // normalised first
  approx(rotate({0, 0, 0, 0}, {1, 2, 3}), {1, 2, 3}); // zero: unchanged
}
