#include <doctest/doctest.h>

#include "app/version.hpp"

#include <highfive/highfive.hpp>

#include <filesystem>
#include <string>
#include <vector>

TEST_CASE("MadsCore is linked and reports a version") {
  std::string v = mads_dash::app::mads_version();
  CHECK_FALSE(v.empty());
}

TEST_CASE("static HDF5 + HighFive round-trip") {
  auto path = std::filesystem::temp_directory_path() / "mads_dash_h5_test.h5";
  std::vector<double> data{1.0, 2.0, 3.0};
  {
    HighFive::File f(path.string(), HighFive::File::Truncate);
    f.createDataSet("/signals/temperature", data);
  }
  {
    HighFive::File f(path.string(), HighFive::File::ReadOnly);
    auto back =
        f.getDataSet("/signals/temperature").read<std::vector<double>>();
    CHECK(back == data);
  } // close before removing: Windows cannot delete an open file
  std::filesystem::remove(path);
}
