#include "net/receiver.hpp"
#include "ui/app_window.hpp"

#include <agent_c.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

int main(int argc, char **argv) {
  const auto started = std::chrono::steady_clock::now();
  // Same options as any mads agent: -s/--settings, -r/--room, --crypto, ...
  // plus --stats and --layout (validated there, read here)
  if (int rc = mads_dash::net::Receiver::handle_cli(argc, argv); rc >= 0)
    return rc;
  mads_dash::ui::AppWindow::Options opts{
      .receiver_args = {argv + 1, argv + argc}};
  opts.started = started;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--stats"))
      opts.stats = true;
    else if (!std::strcmp(argv[i], "--layout") && i + 1 < argc)
      opts.layout = argv[++i];
    else if (!std::strncmp(argv[i], "--layout=", 9))
      opts.layout = argv[i] + 9;
  }
  // Ctrl+C / SIGTERM ask the render loop to quit (see AppWindow::run), so the
  // session is saved; same flag as MADS's own handlers in Agent::loop().
  std::signal(SIGINT, [](int) { mads_stop_process(); });
  std::signal(SIGTERM, [](int) { mads_stop_process(); });
  try {
    mads_dash::ui::AppWindow window(std::move(opts));
    return window.run();
  } catch (const std::exception &e) {
    std::fprintf(stderr, "mads-dashboard: %s\n", e.what());
    return 1;
  }
}
