#include "net/receiver.hpp"
#include "ui/app_window.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char **argv) {
  // Same options as any mads agent: -s/--settings, -r/--room, --crypto, ...
  if (int rc = mads_dash::net::Receiver::handle_cli(argc, argv); rc >= 0)
    return rc;
  try {
    mads_dash::ui::AppWindow window({.receiver_args = {argv + 1, argv + argc}});
    return window.run();
  } catch (const std::exception &e) {
    std::fprintf(stderr, "mads-dashboard: %s\n", e.what());
    return 1;
  }
}
