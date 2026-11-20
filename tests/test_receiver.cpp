#include <doctest/doctest.h>

#include "net/receiver.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>

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
