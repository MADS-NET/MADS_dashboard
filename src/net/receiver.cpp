#include "net/receiver.hpp"

#include <agent_app.hpp>

#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;

namespace mads_dash::net {

namespace {
constexpr const char *kAgentName = "dashboard";

// Same option set for CLI validation and for the network thread.
void add_options(Mads::AgentApp &app) {
  app.options()("topic",
                "Topic filter(s), MQTT-style wildcards allowed "
                "(default: settings, else all)",
                cxxopts::value<std::vector<std::string>>());
  app.add_common_options();
  app.add_agent_identity_options();
  app.raw_options().parse_positional({"topic"});
  app.raw_options().positional_help("[topic ...]");
}

double utc_now() {
  using namespace std::chrono;
  return duration<double>(system_clock::now().time_since_epoch()).count();
}
} // namespace

int Receiver::handle_cli(int argc, char **argv) {
  Mads::AgentApp app(kAgentName, SETTINGS_URI);
  add_options(app);
  auto &parsed = app.parse_options(argc, argv); // exits on invalid options
  return Mads::AgentApp::handle_standard_exit_options<Mads::AgentApp>(
      parsed, app.raw_options(), argv);
}

Receiver::Receiver(std::vector<std::string> args, std::size_t queue_capacity)
    : _args(std::move(args)), _capacity(queue_capacity) {
  // Without a settings timeout a missing broker would block the thread forever.
  if (std::find(_args.begin(), _args.end(), "--settings-timeout") ==
      _args.end()) {
    _args.push_back("--settings-timeout");
    _args.push_back("3000");
  }
  _thread = std::thread([this] { run(); });
}

Receiver::~Receiver() {
  _stop = true;
  if (_thread.joinable())
    _thread.join();
}

std::vector<data::Message> Receiver::drain() {
  std::lock_guard lock(_mutex);
  std::vector<data::Message> out(std::make_move_iterator(_queue.begin()),
                                 std::make_move_iterator(_queue.end()));
  _queue.clear();
  return out;
}

std::string Receiver::status() const {
  std::lock_guard lock(_mutex);
  return _status;
}

void Receiver::set_status(State s, std::string text) {
  std::lock_guard lock(_mutex);
  _status = std::move(text);
  _state = s;
}

void Receiver::run() {
  std::vector<std::string> argv_s{kAgentName};
  argv_s.insert(argv_s.end(), _args.begin(), _args.end());
  std::vector<char *> argv;
  for (auto &a : argv_s)
    argv.push_back(a.data());
  argv.push_back(nullptr);

  try {
    Mads::AgentApp agent(kAgentName, SETTINGS_URI);
    add_options(agent);
    auto &parsed =
        agent.parse_options(static_cast<int>(argv_s.size()), argv.data());
    set_status(State::Connecting, "fetching settings...");
    agent.fetch_settings(parsed);
    agent.add_default_section(); // no [dashboard] section: subscribe to all
    agent.init(parsed, SETTINGS_URI, /*install_watchdog=*/false);
    agent.set_pub_topic(""); // read-only sink: never publish
    if (parsed.count("topic"))
      agent.set_sub_topic(parsed["topic"].as<std::vector<std::string>>());
    agent.set_receive_timeout(100); // ms, so the stop flag is checked
    agent.connect(0ms);
    set_status(State::Connected, agent.sub_endpoint());

    while (!_stop) {
      if (agent.receive() != Mads::message_type::json)
        continue;
      auto [topic, doc] = agent.last_json();
      data::Message msg{std::move(topic), std::move(doc), utc_now()};
      std::lock_guard lock(_mutex);
      if (_queue.size() >= _capacity) {
        _queue.pop_front(); // overload: drop oldest, keep newest
        ++_dropped;
      }
      _queue.push_back(std::move(msg));
    }
    agent.disconnect();
    set_status(State::Stopped, "disconnected");
  } catch (const std::exception &e) {
    set_status(State::Failed, e.what());
  }
}

} // namespace mads_dash::net
