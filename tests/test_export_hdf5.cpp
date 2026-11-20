#include <doctest/doctest.h>

#include "data/export_hdf5.hpp"

#include <highfive/highfive.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mads_dash::data;
using nlohmann::json;

namespace {

Store make_store() {
  Store s;
  for (int i = 0; i < 5; ++i) {
    json p = {{"x", i * 1.5},
              {"on", i % 2 == 0},
              {"name", i < 3 ? "idle" : "run"},
              {"arr", {i, i + 10, i + 20}},
              {"m", {{1, 2, 3}, {4, 5, 6}}}};
    s.ingest({"ts", p, 100.0 + i});
  }
  return s;
}

std::filesystem::path tmp(const char *n) {
  return std::filesystem::temp_directory_path() / n;
}

} // namespace

TEST_CASE("export_hdf5 round-trips every kind") {
  auto store = make_store();
  auto path = tmp("mads_dash_export_all.h5");
  auto st = export_hdf5(store, path, 0, 1000);
  CHECK(st.datasets > 0);
  CHECK(st.samples > 0);
  {
    HighFive::File f(path.string(), HighFive::File::ReadOnly);
    std::string by;
    f.getAttribute("created_by").read(by);
    CHECK(by == "mads-dashboard");
    double t0 = -1, t1 = -1;
    f.getAttribute("t0").read(t0);
    f.getAttribute("t1").read(t1);
    CHECK(t0 == 0);
    CHECK(t1 == 1000);

    auto kind = [&](const std::string &g) {
      std::string k;
      f.getGroup(g).getAttribute("kind").read(k);
      return k;
    };
    CHECK(kind("/ts/x") == "number");
    CHECK(kind("/ts/on") == "bool");
    CHECK(kind("/ts/name") == "string");
    CHECK(kind("/ts/arr") == "array");
    CHECK(kind("/ts/m") == "matrix");

    auto x = f.getDataSet("/ts/x/value").read<std::vector<double>>();
    CHECK(x == std::vector<double>{0, 1.5, 3, 4.5, 6});
    auto t = f.getDataSet("/ts/x/t").read<std::vector<double>>();
    CHECK(t == std::vector<double>{100, 101, 102, 103, 104});
    auto on = f.getDataSet("/ts/on/value").read<std::vector<double>>();
    CHECK(on == std::vector<double>{1, 0, 1, 0, 1});

    // string events: one per change
    auto names =
        f.getDataSet("/ts/name/value").read<std::vector<std::string>>();
    CHECK(names == std::vector<std::string>{"idle", "run"});
    auto nt = f.getDataSet("/ts/name/t").read<std::vector<double>>();
    CHECK(nt == std::vector<double>{100, 103});

    // the element leaves and the whole array coexist
    auto ds = f.getDataSet("/ts/arr/value");
    CHECK(ds.getDimensions() == std::vector<std::size_t>{5, 3});
    auto a = ds.read<std::vector<std::vector<double>>>();
    CHECK(a[4] == std::vector<double>{4, 14, 24});
    CHECK(f.exist("/ts/arr[1]/value"));

    auto m = f.getDataSet("/ts/m/value");
    CHECK(m.getDimensions() == std::vector<std::size_t>{2, 3});
    CHECK(m.read<std::vector<std::vector<double>>>()[1][2] == 6);
    double mt = 0;
    f.getGroup("/ts/m").getAttribute("t").read(mt);
    CHECK(mt == 104);
  }
  std::filesystem::remove(path);
}

TEST_CASE("export_hdf5 filters by time, inclusively") {
  auto store = make_store();
  auto path = tmp("mads_dash_export_time.h5");
  export_hdf5(store, path, 101, 103, {"/ts/x"});
  {
    HighFive::File f(path.string(), HighFive::File::ReadOnly);
    auto t = f.getDataSet("/ts/x/t").read<std::vector<double>>();
    CHECK(t == std::vector<double>{101, 102, 103});
  }
  std::filesystem::remove(path);
}

TEST_CASE("export_hdf5 keypath filter and skipping empty keypaths") {
  auto store = make_store();
  auto path = tmp("mads_dash_export_filter.h5");
  auto st = export_hdf5(store, path, 0, 1000, {"/ts/x", "/ts/missing"});
  CHECK(st.datasets == 2);
  CHECK(st.samples == 5);
  {
    HighFive::File f(path.string(), HighFive::File::ReadOnly);
    CHECK(f.exist("/ts/x"));
    CHECK_FALSE(f.exist("/ts/on"));
    CHECK_FALSE(f.exist("/ts/missing"));
  }
  // nothing in range: no groups at all
  st = export_hdf5(store, path, 500, 600);
  CHECK(st.datasets == 0);
  CHECK(st.samples == 0);
  {
    HighFive::File f(path.string(), HighFive::File::ReadOnly);
    CHECK(f.getNumberObjects() == 0);
  }
  std::filesystem::remove(path);
}

TEST_CASE("export_hdf5 skips array snapshots of a different length") {
  Store s;
  s.ingest({"ts", {{"a", {1, 2, 3}}}, 1.0});
  s.ingest({"ts", {{"a", {1, 2, 3, 4}}}, 2.0});
  s.ingest({"ts", {{"a", {7, 8, 9}}}, 3.0});
  auto path = tmp("mads_dash_export_arr.h5");
  export_hdf5(s, path, 0, 10, {"/ts/a"});
  {
    HighFive::File f(path.string(), HighFive::File::ReadOnly);
    CHECK(f.getDataSet("/ts/a/value").getDimensions() ==
          std::vector<std::size_t>{2, 3});
    CHECK(f.getDataSet("/ts/a/t").read<std::vector<double>>() ==
          std::vector<double>{1, 3});
  }
  std::filesystem::remove(path);
}

TEST_CASE("export_hdf5 throws runtime_error on an unwritable path") {
  auto store = make_store();
  auto path = tmp("mads_dash_no_such_dir") / "sub" / "out.h5";
  CHECK_THROWS_AS(export_hdf5(store, path, 0, 1000), std::runtime_error);
}
