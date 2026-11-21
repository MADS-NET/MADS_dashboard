#include <doctest/doctest.h>

#include "ui/layout.hpp"

#include <filesystem>
#include <fstream>
#include <map>

using namespace mads_dash;
using nlohmann::json;

namespace {
/// Non-default options for each registered type.
const std::map<std::string, json> &non_default() {
  static const std::map<std::string, json> m{
      {"Time series", {{"cursor", true}, {"snap", false}}},
      {"Readout", {{"big", false}}},
      {"XY",
       {{"series",
         {{{{"key", "src/a"}, {"index", -1}},
           {{"key", "src/b[0]"}, {"index", 2}}}}},
        {"fade", false},
        {"markers", true}}},
      {"XYZ",
       {{"series",
         {{{{"key", "src/a"}, {"index", 0}},
           {{"key", "src/a"}, {"index", 1}},
           {{"key", "src/a"}, {"index", 2}}}}},
        {"fade", false},
        {"markers", true},
        {"pose_axes", false},
        {"equal", false}}},
      {"Histogram", {{"bins", 12}, {"density", true}}},
      {"Spectrum", {{"log_y", true}}},
      {"Boxplot", json::object()},
      {"Heatmap",
       {{"auto_range", false},
        {"min", -2.5},
        {"max", 7.0},
        {"colormap", "Plasma"}}},
  };
  return m;
}
} // namespace

TEST_CASE("layout round trip of every registered view type") {
  std::vector<std::unique_ptr<ui::View>> views;
  int id = 3;
  for (const auto &t : ui::view_types()) {
    auto v = t.make(id++);
    v->add_source("src/a");
    v->add_source("src/b[0]");
    auto it = non_default().find(t.name);
    REQUIRE(it != non_default().end());
    v->set_options(it->second);
    views.push_back(std::move(v));
  }
  json j = ui::layout_to_json(views, 42.0, 123.0, "[Window][x]\nPos=1,2\n");
  CHECK(j["format"] == 1);

  std::vector<std::string> warnings;
  ui::Layout l = ui::layout_from_json(j, warnings);
  CHECK(warnings.empty());
  CHECK(l.window == 42.0);
  CHECK(l.buffer == 123.0);
  CHECK(l.imgui_ini == "[Window][x]\nPos=1,2\n");
  REQUIRE(l.views.size() == views.size());
  CHECK(l.next_view_id == id);
  for (std::size_t i = 0; i < views.size(); ++i) {
    CHECK(std::string(l.views[i]->type_name()) == views[i]->type_name());
    CHECK(l.views[i]->id() == views[i]->id());
    CHECK(l.views[i]->sources() == views[i]->sources());
    CHECK(l.views[i]->options() == views[i]->options());
    auto it = non_default().find(views[i]->type_name());
    for (const auto &[k, v] : it->second.items())
      CHECK(l.views[i]->options().at(k) == v);
  }
}

TEST_CASE("layout skips unknown types and malformed entries") {
  json j = {{"format", 1},
            {"views",
             {{{"type", "Nope"}, {"id", 1}},
              42,
              {{"type", "XY"}},
              {{"type", "Readout"}, {"id", 5}, {"sources", {"a", 7}}}}}};
  std::vector<std::string> warnings;
  ui::Layout l = ui::layout_from_json(j, warnings);
  REQUIRE(l.views.size() == 1);
  CHECK(l.views[0]->id() == 5);
  CHECK(l.views[0]->sources() == std::vector<std::string>{"a"});
  CHECK(warnings.size() == 4);
  CHECK(l.next_view_id == 6);
  CHECK(l.window == 10.0);

  warnings.clear();
  ui::Layout bad = ui::layout_from_json(json(7), warnings);
  CHECK(bad.views.empty());
  CHECK(warnings.size() == 1);
}

TEST_CASE("layout ignores options of the wrong type") {
  json j = {{"views",
             {{{"type", "Histogram"},
               {"id", 2},
               {"options", {{"bins", "many"}, {"density", true}}}}}}};
  std::vector<std::string> warnings;
  ui::Layout l = ui::layout_from_json(j, warnings);
  REQUIRE(l.views.size() == 1);
  CHECK(l.views[0]->options()["bins"] == 30);
  CHECK(l.views[0]->options()["density"] == true);
}

TEST_CASE("layout file save and load") {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "mads_dash_test_layout";
  fs::remove_all(dir);
  fs::path file = dir / "sub" / "layout.json";
  json j = {{"format", 1}, {"window", 5.0}, {"views", json::array()}};
  ui::save_layout_file(file, j);
  CHECK(ui::load_layout_file(file) == j);

  CHECK_THROWS_AS(ui::load_layout_file(dir / "missing.json"),
                  std::runtime_error);
  ui::save_layout_file(dir / "bad.json", j);
  {
    std::ofstream(dir / "bad.json") << "{not json";
  }
  CHECK_THROWS_AS(ui::load_layout_file(dir / "bad.json"), std::runtime_error);
  CHECK_THROWS_AS(ui::save_layout_file(dir / "sub" / "layout.json" / "x", j),
                  std::runtime_error);
  fs::remove_all(dir);
}

TEST_CASE("session_file ends with mads-dashboard/session.json") {
  auto p = ui::session_file();
  CHECK(p.filename() == "session.json");
  CHECK(p.parent_path().filename() == "mads-dashboard");
}
