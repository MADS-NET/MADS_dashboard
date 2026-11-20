#include <doctest/doctest.h>

#include "ui/views/histogram_view.hpp"

using namespace mads_dash;

TEST_CASE("window_values") {
  data::Series s;
  s.kind = data::Kind::Number;
  s.t = {1, 2, 3, 4, 5};
  s.v = {10, 20, 30, 40, 50};

  SUBCASE("bounds are inclusive") {
    CHECK(ui::window_values(s, 2, 4) == std::vector<double>{20, 30, 40});
  }
  SUBCASE("whole range and partial overlap") {
    CHECK(ui::window_values(s, 0, 100).size() == 5);
    CHECK(ui::window_values(s, 4.5, 100) == std::vector<double>{50});
  }
  SUBCASE("empty window") {
    CHECK(ui::window_values(s, 2.2, 2.8).empty());
    CHECK(ui::window_values(s, 6, 7).empty());
    CHECK(ui::window_values(s, 4, 2).empty());
  }
  SUBCASE("empty series") {
    CHECK(ui::window_values(data::Series{}, 0, 10).empty());
  }
}
