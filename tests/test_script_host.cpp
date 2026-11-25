#include <doctest/doctest.h>

#include "script/script_host.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <numbers>

using mads_dash::data::Message;
using mads_dash::script::ScriptHost;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {

const char *kIdentity = "topics = {''}\nfunction process(_, m) return m end";

// Feeds one message to every enabled script.
std::vector<Message> run(ScriptHost &h, const json &payload,
                         const std::string &topic = "a", double t = 1) {
  std::vector<Message> out;
  h.on_message({topic, payload, 42.5}, t, out);
  return out;
}

ScriptHost host_with(const std::string &name, const std::string &src) {
  ScriptHost h;
  h.load(name, src);
  h.set_enabled(name, true);
  return h;
}

struct TempDir {
  fs::path p;
  TempDir() {
    p = fs::temp_directory_path() /
        ("dash_script_test_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(p);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(p, ec);
  }
};

void write(const fs::path &p, const std::string &s) {
  std::ofstream(p, std::ios::binary) << s;
}

} // namespace

TEST_CASE("JSON and Lua round trip") {
  auto h = host_with("id", kIdentity);
  const json in = {{"obj", {{"a", 1}, {"b", {{"c", "str"}}}}},
                   {"nums", {1.5, 2.5, 3.5}},
                   {"ints", {1, 2, 3}},
                   {"matrix", {{1, 2}, {3, 4}}},
                   {"flag", true},
                   {"off", false},
                   {"s", "hello"},
                   {"i", 7},
                   {"f", 0.25}};
  const auto out = run(h, in);
  REQUIRE(out.size() == 1);
  CHECK(out[0].payload == in);
  CHECK(out[0].payload["i"].is_number_integer());
  CHECK(out[0].payload["f"].is_number_float());
  CHECK(out[0].payload["matrix"][1][0] == 3);
}

TEST_CASE("outputs: nil, scalar, topic, time") {
  auto h = host_with("x", "topics={''}\nfunction process(t, m) "
                          "if m.skip then return nil end return m.n end");
  CHECK(run(h, {{"skip", true}}).empty());
  const auto out = run(h, {{"n", 3}});
  REQUIRE(out.size() == 1);
  CHECK(out[0].topic == "~x");
  CHECK(out[0].payload == json{{"value", 3}});
  CHECK(out[0].recv_time == 42.5);
}

TEST_CASE("state is kept across calls") {
  auto h = host_with("c", "topics={''}\nlocal n=0\n"
                          "function process() n=n+1 return {n=n} end");
  run(h, {});
  run(h, {});
  CHECK(run(h, {})[0].payload["n"] == 3);
  CHECK(h.scripts()[0].calls == 3);
}

TEST_CASE("topic matching") {
  ScriptHost h;
  h.load("m", "topics={'test/+'}\nfunction process() return 1 end");
  h.load("n", "topics={'other'}\nfunction process() return 1 end");
  h.load("all", "topics={''}\nfunction process() return 1 end");
  h.load("none", "function process() return 1 end");
  for (const char *n : {"m", "n", "all", "none"})
    h.set_enabled(n, true);
  const auto names = [&](const std::string &topic) {
    std::string s;
    for (const auto &m : run(h, {}, topic))
      s += m.topic + " ";
    return s;
  };
  CHECK(names("test/a") == "~all ~m ");
  CHECK(names("other") == "~all ~n ");
  CHECK(names("~x").empty());
}

TEST_CASE("syntax error") {
  ScriptHost h;
  h.load("bad", "function process(");
  const auto &s = h.scripts()[0];
  CHECK(s.error.find("bad.lua:") != std::string::npos);
  CHECK_FALSE(s.L);
  h.set_enabled("bad", true);
  CHECK_FALSE(h.scripts()[0].enabled);
  h.load("nop", "x = 1");
  CHECK(h.scripts()[1].error.find("no process()") != std::string::npos);
}

TEST_CASE("runtime error disables, enabling clears") {
  auto h =
      host_with("r", "topics={''}\n\nfunction process() error('boom') end");
  CHECK(run(h, {}).empty());
  const auto &s = h.scripts()[0];
  CHECK(s.error.find("r.lua:3:") != std::string::npos);
  CHECK(s.error.find("boom") != std::string::npos);
  CHECK_FALSE(s.enabled);
  h.set_enabled("r", true);
  CHECK(h.scripts()[0].enabled);
  CHECK(h.scripts()[0].error.empty());
}

TEST_CASE("instruction budget") {
  auto h = host_with("loop", "topics={''}\nfunction process() "
                             "while true do end end");
  const auto t0 = std::chrono::steady_clock::now();
  run(h, {});
  CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(2));
  CHECK(h.scripts()[0].error.find("instruction budget") != std::string::npos);
  CHECK_FALSE(h.scripts()[0].enabled);
  ScriptHost g;
  g.load("top", "while true do end function process() end");
  CHECK(g.scripts()[0].error.find("instruction budget") != std::string::npos);
}

TEST_CASE("a cyclic table is cut at the depth limit, without an error") {
  auto h = host_with("cyc", "topics={''}\nfunction process() "
                            "local t = {x = 1} t.t = t return t end");
  const auto out = run(h, {});
  REQUIRE(out.size() == 1);
  CHECK(out[0].payload["x"] == 1);
  CHECK(out[0].payload["t"]["t"]["x"] == 1);
  CHECK(h.scripts()[0].error.empty());
  CHECK(h.scripts()[0].enabled);
}

TEST_CASE("sandbox") {
  auto h = host_with(
      "sb", "topics={''}\nfunction process() return {ok = io == nil and "
            "os.execute == nil and require == nil and dofile == nil and "
            "type(os.clock()) == 'number'} end");
  const auto out = run(h, {});
  REQUIRE(out.size() == 1);
  CHECK(out[0].payload["ok"] == true);
}

TEST_CASE("reload keeps enabled") {
  auto h = host_with("k", kIdentity);
  h.load("k", "topics={''}\nfunction process() return 5 end");
  CHECK(h.enabled() == std::vector<std::string>{"k"});
  CHECK(run(h, {})[0].payload["value"] == 5);
  CHECK(h.scripts()[0].calls == 1);
}

TEST_CASE("scan") {
  TempDir d;
  ScriptHost h;
  h.scan(d.p / "missing");
  CHECK(h.scripts().empty());
  h.load("inline", kIdentity);
  const auto file = d.p / "one.lua";
  write(file, "topics={''}\nfunction process() return 1 end");
  write(d.p / "note.txt", "x");
  h.scan(d.p);
  REQUIRE(h.scripts().size() == 2);
  CHECK(h.scripts()[1].name == "one");
  CHECK(h.scripts()[1].error.empty());

  write(file, "function process(");
  fs::last_write_time(file, fs::last_write_time(file) + std::chrono::hours(1));
  h.scan(d.p);
  CHECK_FALSE(h.scripts()[1].error.empty());

  fs::remove(file);
  h.scan(d.p);
  REQUIRE(h.scripts().size() == 1);
  CHECK(h.scripts()[0].name == "inline");
}

TEST_CASE("dash.fft finds a sine") {
  auto h = host_with(
      "f", "topics={''}\nfunction process()\n"
           "local t, v = {}, {}\n"
           "for i = 0, 255 do t[#t+1] = i / 100 "
           "v[#v+1] = math.sin(2 * math.pi * 5 * i / 100) end\n"
           "local s = dash.fft(t, v)\n"
           "local k = 1\n"
           "for i = 1, #s.amp do if s.amp[i] > s.amp[k] then k = i end end\n"
           "return {peak = s.f[k], n = #s.f, same = #s.f == #s.amp}\nend");
  const auto out = run(h, {});
  REQUIRE(out.size() == 1);
  CHECK(out[0].payload["peak"].get<double>() ==
        doctest::Approx(5).epsilon(0.1));
  CHECK(out[0].payload["same"] == true);

  h.load("g", "topics={''}\nfunction process() dash.fft({1,2,3,4},{1}) end");
  h.set_enabled("g", true);
  run(h, {});
  CHECK(h.scripts()[1].error.find("same length") != std::string::npos);
}

TEST_CASE("example scripts run on testsource-like messages") {
  const auto &ex = mads_dash::script::example_scripts();
  REQUIRE(ex.size() >= 5);
  for (const auto &[file, code] : ex) {
    CAPTURE(file);
    std::string name = file;
    name.erase(name.size() - 4);
    ScriptHost h;
    h.load(name, code);
    h.set_enabled(name, true);
    REQUIRE(h.scripts()[0].error.empty());
    const char *states[] = {"idle", "run", "hold"};
    std::size_t outputs = 0;
    for (int i = 0; i < 200; ++i) {
      const double t = 1000 + i * 0.01, w = 2 * std::numbers::pi * t;
      const json msg = {
          {"accel", {std::sin(w), std::sin(2 * w), std::sin(3 * w)}},
          {"position", {std::cos(w), std::sin(w), 0.5}},
          {"pose", {std::cos(w), std::sin(w), 0.5, 0.9, 0.0, 0.0, 0.43}},
          {"xy", {{"x", {0, 1, 2}}, {"y", {0, 1, 0}}}},
          {"map", {{1, 2}, {3, 4}}},
          {"state", states[(i / 50) % 3]},
          {"alarm", i % 7 == 0},
          {"counter", i}};
      std::vector<Message> out;
      h.on_message({"testsource", msg, t}, t, out);
      CAPTURE(i);
      REQUIRE(h.scripts()[0].error.empty());
      for (const auto &m : out)
        CHECK(m.topic == "~" + name);
      outputs += out.size();
    }
    CHECK(outputs > 0);
  }
}
