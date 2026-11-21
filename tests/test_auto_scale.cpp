#include <doctest/doctest.h>

#include "ui/auto_scale.hpp"

using mads_dash::ui::StickyRange;

TEST_CASE("sticky range grows by half the data range and then sticks") {
  StickyRange r;
  CHECK(r.update(0, 10));
  CHECK(r.lo == -5);
  CHECK(r.hi == 15);
  CHECK_FALSE(r.update(-4, 14)); // still inside: unchanged
  CHECK(r.lo == -5);
  CHECK(r.update(0, 20)); // leaves it: refit to the new data range
  CHECK(r.lo == -10);
  CHECK(r.hi == 30);
}

TEST_CASE("sticky range edge cases") {
  StickyRange r;
  CHECK(r.update(3, 3)); // flat data widen by |value|
  CHECK(r.lo == 0);
  CHECK(r.hi == 6);
  r.reset();
  CHECK(r.update(0, 0));
  CHECK(r.lo == -1);
  CHECK(r.hi == 1);
  CHECK_FALSE(r.update(1, 0)); // empty input ignored
  CHECK_FALSE(r.update(0, 1.0 / 0.0));
}
