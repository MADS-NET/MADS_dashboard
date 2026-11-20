#include <doctest/doctest.h>

#include "ui/views/time_series_view.hpp"

#include <cmath>

using namespace mads_dash;

static data::Series make_series() {
  data::Series s;
  s.kind = data::Kind::Number;
  s.t = {1.0, 2.0, 4.0};
  s.v = {10.0, 20.0, 40.0};
  return s;
}

TEST_CASE("value_at interpolates and clamps") {
  const auto s = make_series();
  CHECK(ui::value_at(s, 1.5) == doctest::Approx(15.0));
  CHECK(ui::value_at(s, 3.0) == doctest::Approx(30.0));
  CHECK(ui::value_at(s, 2.0) == doctest::Approx(20.0));
  CHECK(ui::value_at(s, 0.0) == doctest::Approx(10.0));
  CHECK(ui::value_at(s, 9.0) == doctest::Approx(40.0));
  CHECK(std::isnan(ui::value_at(data::Series{}, 1.0)));
}

TEST_CASE("snap_time picks the nearest sample") {
  const auto s = make_series();
  CHECK(ui::snap_time(s, 1.4) == doctest::Approx(1.0));
  CHECK(ui::snap_time(s, 1.6) == doctest::Approx(2.0));
  CHECK(ui::snap_time(s, 3.1) == doctest::Approx(4.0));
  CHECK(ui::snap_time(s, -5.0) == doctest::Approx(1.0));
  CHECK(ui::snap_time(s, 99.0) == doctest::Approx(4.0));
  CHECK(std::isnan(ui::snap_time(data::Series{}, 1.0)));
}
