#include <doctest/doctest.h>

#include "net/receiver.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using mads_dash::net::Receiver;

// Needs a running broker + testsource; enabled with MADS_DASH_LIVE=1.
TEST_CASE("receiver gets testsource messages from a live broker" *
          doctest::skip(!std::getenv("MADS_DASH_LIVE"))) {
  Receiver r({"-s", "tcp://localhost:9092", "testsource"});
  std::size_t n = 0;
  for (int i = 0; i < 30 && n < 10; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (auto &m : r.drain()) {
      CHECK(m.topic == "testsource");
      CHECK(m.payload.contains("accel"));
      ++n;
    }
  }
  INFO("status: " << r.status());
  CHECK(r.state() == Receiver::State::Connected);
  CHECK(n >= 10);
}

TEST_CASE("receiver reports failure without a broker instead of hanging" *
          doctest::skip(!std::getenv("MADS_DASH_LIVE"))) {
  Receiver r({"-s", "tcp://localhost:9999", "--settings-timeout", "500"});
  for (int i = 0; i < 30 && r.state() == Receiver::State::Connecting; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  CHECK(r.state() == Receiver::State::Failed);
}

// Reconnect after a failed first attempt: run with MADS_DASH_RECONNECT=1 and
// start the broker + testsource ~5 s after the test starts (see the script in
// the commit that added this test).
TEST_CASE("receiver reconnects after a first attempt without broker" *
          doctest::skip(!std::getenv("MADS_DASH_RECONNECT"))) {
  const char *how = std::getenv("MADS_DASH_RECONNECT");
  {
    Receiver r1({"-s", "tcp://localhost:9092"});
    for (int i = 0; i < 60 && r1.state() == Receiver::State::Connecting; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    MESSAGE("first attempt: " << (int)r1.state() << " " << r1.status());
  }
  std::this_thread::sleep_for(std::chrono::seconds(6)); // broker starts now
  std::vector<std::string> args =
      std::string(how) == "room"
          ? std::vector<std::string>{"--room=mads"}
          : std::vector<std::string>{"-s", "tcp://localhost:9092"};
  Receiver r2(args);
  std::size_t n = 0;
  for (int i = 0; i < 60 && n < 10; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    n += r2.drain().size();
  }
  MESSAGE("second attempt: " << (int)r2.state() << " " << r2.status()
                             << ", messages " << n);
  CHECK(n >= 10);
}

TEST_CASE("connect_args: one argument per option with an implicit value") {
  using mads_dash::net::connect_args;
  using mads_dash::net::ConnectForm;
  ConnectForm f;
  f.topics = "a b/#";
  CHECK(connect_args(f) ==
        std::vector<std::string>{"-s", "tcp://localhost:9092", "a", "b/#"});
  f.by_room = true;
  f.room = "lab";
  f.topics = "";
  CHECK(connect_args(f) == std::vector<std::string>{"--room=lab"});
  f.crypto = true;
  f.keys_dir = "/k";
  CHECK(connect_args(f) ==
        std::vector<std::string>{"--room=lab", "--crypto", "--keys_dir=/k",
                                 "--key_client=client", "--key_broker=broker"});
}

TEST_CASE("missing_key_files lists the CURVE files that are not there") {
  using mads_dash::net::ConnectForm;
  using mads_dash::net::missing_key_files;
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "mads_dash_keys_test";
  fs::create_directories(dir);
  ConnectForm f;
  CHECK(missing_key_files(f).empty()); // crypto off
  f.crypto = true;
  f.keys_dir = dir.string();
  CHECK(missing_key_files(f).size() == 3);
  for (const char *n : {"client.key", "client.pub", "broker.pub"})
    std::ofstream(dir / n) << "x";
  CHECK(missing_key_files(f).empty());
  fs::remove_all(dir);
}
