#include "net/receiver.hpp"
#include "ui/app_window.hpp"

#include <agent_c.h>

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h> // _NSGetExecutablePath
#endif
extern char **environ;
#endif

namespace {
// Return to the shell at once: re-launch this program as a detached child
// (a plain fork() crashes on macOS, where the ObjC runtime is already loaded)
// and return true in the parent, which must exit. The child's output is
// discarded. If the child cannot be started, return false and run in this
// process.
bool detach(char **argv) {
  constexpr const char *kChild = "MADS_DASHBOARD_DETACHED";
  if (std::getenv(kChild))
    return false; // already the detached child
#if defined(_WIN32)
  _putenv_s(kChild, "1");
  wchar_t exe[MAX_PATH];
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  STARTUPINFOW si{sizeof si};
  PROCESS_INFORMATION pi{};
  // DETACHED_PROCESS: no console, so stdout/stderr go nowhere
  if (!CreateProcessW(exe, GetCommandLineW(), nullptr, nullptr, FALSE,
                      DETACHED_PROCESS, nullptr, nullptr, &si, &pi))
    return false;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
#else
  setenv(kChild, "1", 1);
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY,
                                   0);
  posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY,
                                   0);
  posix_spawn_file_actions_adddup2(&fa, STDOUT_FILENO, STDERR_FILENO);
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  // new session: no SIGHUP/SIGINT from the shell's terminal
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
  // the executable itself, not argv[0]: `mads dashboard` runs us as
  // "dashboard", which is not on the PATH
#if defined(__APPLE__)
  char exe[4096];
  std::uint32_t size = sizeof exe;
  if (_NSGetExecutablePath(exe, &size) != 0)
    std::snprintf(exe, sizeof exe, "%s", argv[0]);
#else
  const char *exe = "/proc/self/exe";
#endif
  pid_t pid;
  const int rc = posix_spawn(&pid, exe, &fa, &attr, argv, environ);
  posix_spawnattr_destroy(&attr);
  posix_spawn_file_actions_destroy(&fa);
  unsetenv(kChild);
  return rc == 0;
#endif
}
} // namespace

int main(int argc, char **argv) {
  const auto started = std::chrono::steady_clock::now();
  // Same options as any mads agent: -s/--settings, -r/--room, --crypto, ...
  // plus --stats and --layout (validated there, read here)
  if (int rc = mads_dash::net::Receiver::handle_cli(argc, argv); rc >= 0)
    return rc;
  mads_dash::ui::AppWindow::Options opts{
      .receiver_args = {argv + 1, argv + argc}};
  opts.started = started;
  bool verbose = false;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--verbose"))
      verbose = true;
    else if (!std::strcmp(argv[i], "--stats"))
      opts.stats = verbose = true; // stats are useless if discarded
    else if (!std::strcmp(argv[i], "--layout") && i + 1 < argc)
      opts.layout = argv[++i];
    else if (!std::strncmp(argv[i], "--layout=", 9))
      opts.layout = argv[i] + 9;
  }
  if (!verbose && detach(argv)) // --verbose: stay in the foreground
    return 0;
  // Ctrl+C (SIGINT) / SIGTERM ask the render loop to quit (see AppWindow::run), so the
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
