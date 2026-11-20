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

TEST_CASE("rotation preserves lengths and right angles") {
  auto dot = [](std::array<double, 3> a, std::array<double, 3> b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  };
  for (int k = 0; k < 50; ++k) {
    // arbitrary, non-normalised quaternions
    const std::array<double, 4> q{std::cos(k * 0.7) + 0.1, std::sin(k * 1.3),
                                  std::cos(k * 2.1), std::sin(k * 0.4) * 2};
    const auto ex = rotate(q, {1, 0, 0}), ey = rotate(q, {0, 1, 0}),
               ez = rotate(q, {0, 0, 1});
    CHECK(dot(ex, ex) == doctest::Approx(1).epsilon(1e-12));
    CHECK(dot(ey, ey) == doctest::Approx(1).epsilon(1e-12));
    CHECK(dot(ez, ez) == doctest::Approx(1).epsilon(1e-12));
    CHECK(std::abs(dot(ex, ey)) < 1e-12);
    CHECK(std::abs(dot(ey, ez)) < 1e-12);
    CHECK(std::abs(dot(ex, ez)) < 1e-12);
  }
}
