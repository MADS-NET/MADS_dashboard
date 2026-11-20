#include <doctest/doctest.h>

#include "ui/views/spectrum_view.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

using mads_dash::ui::spectrum;

TEST_CASE("spectrum of a 5 Hz sine") {
  std::vector<double> t, v;
  for (int i = 0; i < 400; ++i) {
    t.push_back(i / 100.0);
    v.push_back(2 * std::sin(2 * std::numbers::pi * 5 * t.back()) + 3);
  }
  const auto s = spectrum(t, v);
  REQUIRE(s.f.size() == s.amp.size());
  REQUIRE(s.f.size() > 2);
  const auto it = std::max_element(s.amp.begin(), s.amp.end());
  const auto k = it - s.amp.begin();
  const double bin = s.f[1] - s.f[0];
  MESSAGE("peak f=", s.f[k], " amp=", *it);
  CHECK(std::abs(s.f[k] - 5.0) <= bin);
  CHECK(*it == doctest::Approx(2.0).epsilon(0.1));
}

TEST_CASE("spectrum of a constant is near zero") {
  std::vector<double> t, v;
  for (int i = 0; i < 64; ++i) {
    t.push_back(i * 0.01);
    v.push_back(7.0);
  }
  const auto s = spectrum(t, v);
  REQUIRE_FALSE(s.amp.empty());
  CHECK(*std::max_element(s.amp.begin(), s.amp.end()) < 1e-9);
}

TEST_CASE("spectrum with fewer than 4 samples is empty") {
  const auto s = spectrum({0, 1, 2}, {1, 2, 3});
  CHECK(s.f.empty());
  CHECK(s.amp.empty());
}
