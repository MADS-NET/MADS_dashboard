#include "ui/views/readout_view.hpp"

#include <doctest/doctest.h>

using mads_dash::data::Kind;
using mads_dash::data::Series;
using mads_dash::ui::readout_text;

TEST_CASE("readout_text numbers") {
  Series s;
  s.kind = Kind::Number;
  s.t = {1.0, 2.0};
  s.v = {3.14159265, 42};
  CHECK(readout_text(s, 1.5) == "3.14159");
  CHECK(readout_text(s, 2.0) == "42");
  CHECK(readout_text(s, 9.0) == "42");
}

TEST_CASE("readout_text before first sample") {
  Series s;
  s.kind = Kind::Number;
  s.t = {1.0};
  s.v = {5};
  CHECK(readout_text(s, 0.5) == "-");
  CHECK(readout_text(Series{}, 1.0) == "-");
}

TEST_CASE("readout_text bools") {
  Series s;
  s.kind = Kind::Bool;
  s.t = {1.0, 2.0};
  s.v = {1, 0};
  CHECK(readout_text(s, 1.5) == "true");
  CHECK(readout_text(s, 2.5) == "false");
}

TEST_CASE("readout_text strings") {
  Series s;
  s.kind = Kind::String;
  s.events = {{1.0, "idle"}, {3.0, "running"}};
  CHECK(readout_text(s, 2.0) == "idle");
  CHECK(readout_text(s, 3.0) == "running");
  CHECK(readout_text(s, 0.0) == "-");
}

TEST_CASE("readout_text arrays and truncation") {
  Series s;
  s.kind = Kind::Array;
  s.snaps.push_back({1.0, {1, 2.5, 3}});
  s.snaps.push_back({2.0, {1, 2, 3, 4, 5, 6, 7, 8}});
  s.snaps.push_back({3.0, {1, 2, 3, 4, 5, 6, 7, 8, 9}});
  CHECK(readout_text(s, 1.5) == "[1, 2.5, 3]");
  CHECK(readout_text(s, 2.5) == "[1, 2, 3, 4, 5, 6, 7, 8]");
  CHECK(readout_text(s, 3.5) == "[1, 2, 3, 4, 5, 6, 7, 8, ...]");
  CHECK(readout_text(s, 0.5) == "-");
}
