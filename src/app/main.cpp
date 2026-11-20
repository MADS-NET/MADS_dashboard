#include "app/version.hpp"
#include "ui/app_window.hpp"

#include <cstdio>
#include <cstring>
#include <exception>

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--version")) {
      std::printf("mads-dashboard %s (MADS %s)\n", mads_dash::app::kVersion,
                  mads_dash::app::mads_version());
      return 0;
    }
  }
  try {
    mads_dash::ui::AppWindow window({});
    return window.run();
  } catch (const std::exception &e) {
    std::fprintf(stderr, "mads-dashboard: %s\n", e.what());
    return 1;
  }
}
