#pragma once

#include "data/store.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mads_dash::net {

/// MADS sink agent running on its own thread. Configured with the same
/// command-line options as any mads-* agent (parsed by Mads::AgentApp:
/// -s/--settings, -r/--room, --crypto, key options, -n/--name, topics...).
/// Received JSON messages are queued; the UI thread drains them each frame.
class Receiver {
public:
  enum class State { Connecting, Connected, Failed, Stopped };

  /// `args` excludes argv[0]. Starts the network thread immediately.
  explicit Receiver(std::vector<std::string> args,
                    std::size_t queue_capacity = 100'000);
  ~Receiver(); ///< stops and joins the thread
  Receiver(const Receiver &) = delete;
  Receiver &operator=(const Receiver &) = delete;

  /// Moves all queued messages out (UI thread).
  std::vector<data::Message> drain();

  State state() const { return _state.load(); }
  std::string status() const; ///< human-readable endpoint or error
  std::size_t dropped() const { return _dropped.load(); }
  const std::vector<std::string> &args() const { return _args; }

  /// Validates CLI args and handles --help/--version. Returns an exit code to
  /// stop with, or -1 to continue. Call once from main() before the GUI.
  static int handle_cli(int argc, char **argv);

private:
  void run();
  void set_status(State s, std::string text);

  std::vector<std::string> _args;
  std::size_t _capacity;
  std::atomic<bool> _stop{false};
  std::atomic<State> _state{State::Connecting};
  std::atomic<std::size_t> _dropped{0};
  mutable std::mutex _mutex; // guards _queue and _status
  // shortcut: mutex-guarded deque, fine at the 10k msg/s target; go lock-free
  // only if profiling shows contention
  std::deque<data::Message> _queue;
  std::string _status;
  std::thread _thread;
};

} // namespace mads_dash::net
