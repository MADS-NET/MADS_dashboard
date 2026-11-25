#pragma once

#include "data/store.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct lua_State;

namespace mads_dash::script {

/// One user script: a Lua file whose `process(topic, msg, t)` turns messages
/// of its input `topics` into messages of the derived topic `~<name>`.
struct Script {
  std::string name;              ///< file stem; output topic is "~" + name
  std::filesystem::path path;    ///< empty for scripts loaded from a string
  std::filesystem::file_time_type mtime{};
  std::vector<std::string> topics; ///< MADS subscription entries ("" = all)
  bool enabled = false;
  std::string error;   ///< last load or runtime error (empty = ok)
  std::size_t calls = 0;
  double total_us = 0; ///< time spent in process(), for the Scripts panel
  std::shared_ptr<lua_State> L; ///< null when the script failed to load
};

/// Loads and runs user scripts. UI thread only, like the Store.
class ScriptHost {
public:
  /// (Re)loads a script from source. Keeps `enabled` of an existing script of
  /// the same name; a new one starts disabled. Errors go to Script::error.
  void load(const std::string &name, const std::string &source,
            const std::filesystem::path &path = {});
  /// Syncs with the `*.lua` files of `dir`: loads new ones, reloads those
  /// whose mtime changed, drops scripts whose file is gone. Cheap when nothing
  /// changed; called about once a second.
  void scan(const std::filesystem::path &dir);
  /// Runs every enabled, loaded script whose topics match `msg.topic` (never
  /// derived `~` topics) and appends its result to `out` as a message of topic
  /// `~<name>`. A runtime error (or a call exceeding the instruction budget)
  /// sets Script::error and disables that script.
  void on_message(const data::Message &msg, double t,
                  std::vector<data::Message> &out);

  void set_enabled(const std::string &name, bool on);
  std::vector<std::string> enabled() const;
  const std::vector<Script> &scripts() const { return _scripts; }

private:
  Script *find(const std::string &name);
  std::vector<Script> _scripts; ///< sorted by name
};

/// The example scripts embedded in the binary: {file name, source}.
const std::vector<std::pair<const char *, const char *>> &example_scripts();

/// `<config dir>/mads-dashboard/scripts` (next to session.json). On first use,
/// when the folder does not exist, it is created with the example scripts.
std::filesystem::path scripts_dir();

} // namespace mads_dash::script
