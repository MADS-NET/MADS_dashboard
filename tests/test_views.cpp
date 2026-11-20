#include <doctest/doctest.h>

#include "ui/im_assert.hpp"
#include "ui/view.hpp"

#include <imgui.h>
#include <implot.h>
#include <implot3d.h>

#include <cmath>

using namespace mads_dash;
using nlohmann::json;

namespace {
const ui::ViewType &type_named(const std::string &name) {
  for (const auto &t : ui::view_types())
    if (name == t.name)
      return t;
  FAIL("no view type " << name);
  return ui::view_types().front();
}

/// ImGui + ImPlot without a window or GPU: frames are built and rendered to
/// draw lists only, so ImGui/ImPlot asserts still fire.
struct HeadlessImGui {
  HeadlessImGui() {
    dash_im_assert_throws = true;
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImPlot3D::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280, 800);
    io.DeltaTime = 1.0f / 60;
    io.IniFilename = nullptr;
    io.Fonts->Build();
  }
  ~HeadlessImGui() {
    dash_im_assert_throws = false;
    ImPlot3D::DestroyContext();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
  }
  template <class F> void frame(F &&body) {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(800, 600));
    ImGui::Begin("view");
    body();
    ImGui::End();
    ImGui::Render();
  }
};

/// A store fed with testsource-like messages: every kind of leaf.
data::Store sample_store() {
  data::Store st;
  for (int i = 0; i < 300; ++i) {
    const double t = 100 + i * 0.01;
    json xs = json::array(), ys = json::array(), map = json::array();
    for (int k = 0; k < 16; ++k) {
      xs.push_back(k * 0.4);
      ys.push_back(std::sin(k * 0.4 + t));
    }
    for (int r = 0; r < 8; ++r)
      map.push_back(json::array({r, r + 1.0, r + 2.0, r + 3.0}));
    st.ingest({"ts",
               {{"timecode", t},
                {"accel", {std::sin(t), std::cos(t), 0.1 * t}},
                {"pose",
                 {std::sin(t), std::cos(t), 0.1 * t, std::cos(0.1 * t), 0.0,
                  0.0, std::sin(0.1 * t)}},
                {"temp", 20 + std::sin(t)},
                {"alarm", i % 50 < 25},
                {"state", i % 100 < 50 ? "idle" : "run"},
                {"xy", {{"x", xs}, {"y", ys}}},
                {"map", map}},
               0});
  }
  return st;
}
} // namespace

TEST_CASE("every view type renders every keypath it accepts, live and paused") {
  HeadlessImGui gui;
  const data::Store store = sample_store();
  for (const auto &type : ui::view_types()) {
    for (const auto &[key, s] : store.series()) {
      if (!type.accepts(s.kind))
        continue;
      INFO(type.name << " <- " << key);
      auto v = type.make(1);
      v->add_source(key);
      for (bool paused : {false, true}) {
        ui::Timeline tl{paused, 10.0, store.latest_time()};
        for (int f = 0; f < 3; ++f)
          gui.frame([&] { v->draw(store, tl); });
      }
    }
  }
}

TEST_CASE("XY view handles its two-source cases and empty sources") {
  HeadlessImGui gui;
  const data::Store store = sample_store();
  ui::Timeline tl{false, 10.0, store.latest_time()};
  for (auto pair : {std::pair{"/ts/xy/x", "/ts/xy/y"},
                    std::pair{"/ts/accel[0]", "/ts/accel[1]"},
                    std::pair{"/ts/missing", "/ts/temp"}}) {
    auto v = type_named("XY").make(1);
    REQUIRE(std::string(v->type_name()) == "XY");
    v->add_source(pair.first);
    v->add_source(pair.second);
    gui.frame([&] { v->draw(store, tl); });
  }
  auto empty = type_named("XY").make(2);
  gui.frame([&] { empty->draw(store, tl); });
}

TEST_CASE("headless harness turns ImGui misuse into a test failure") {
  HeadlessImGui gui;
  CHECK_THROWS_AS(gui.frame([] { ImGui::PopID(); }), std::logic_error);
}

TEST_CASE("XYZ view: three numbers, short arrays and empty sources") {
  HeadlessImGui gui;
  const data::Store store = sample_store();
  ui::Timeline tl{false, 10.0, store.latest_time()};
  auto v = type_named("XYZ").make(1);
  for (auto k : {"/ts/accel[0]", "/ts/accel[1]", "/ts/accel[2]"})
    v->add_source(k);
  gui.frame([&] { v->draw(store, tl); });
  auto xy_only = type_named("XYZ").make(2); // [x, y] arrays are too short
  xy_only->add_source("/ts/xy/x");
  gui.frame([&] { xy_only->draw(store, tl); });
  auto empty = type_named("XYZ").make(3);
  gui.frame([&] { empty->draw(store, tl); });
}
