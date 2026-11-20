#include "data/store.hpp"

#include <algorithm>
#include <functional>

using nlohmann::json;

namespace mads_dash::data {

namespace {
bool numeric_array(const json &j) {
  return j.is_array() && !j.empty() &&
         std::all_of(j.begin(), j.end(),
                     [](const json &e) { return e.is_number(); });
}
} // namespace

Kind kind_of(const json &j) {
  if (j.is_boolean())
    return Kind::Bool;
  if (j.is_number())
    return Kind::Number;
  if (j.is_string())
    return Kind::String;
  if (numeric_array(j))
    return Kind::Array;
  if (j.is_array() && !j.empty() &&
      std::all_of(j.begin(), j.end(), [&](const json &r) {
        return numeric_array(r) && r.size() == j.front().size();
      }))
    return Kind::Matrix;
  return Kind::Other;
}

void flatten(const std::string &prefix, const json &j,
             const std::function<void(const std::string &, const json &)> &leaf,
             std::size_t max_elements) {
  if (j.is_object()) {
    for (auto &[k, v] : j.items())
      flatten(prefix + "/" + k, v, leaf, max_elements);
    return;
  }
  const Kind k = kind_of(j);
  if (k == Kind::Other && j.is_array()) {
    for (std::size_t i = 0; i < j.size(); ++i)
      flatten(prefix + "[" + std::to_string(i) + "]", j[i], leaf, max_elements);
    return;
  }
  if (k == Kind::Other)
    return; // null, empty array/object
  leaf(prefix, j);
  if (k == Kind::Array && j.size() <= max_elements)
    for (std::size_t i = 0; i < j.size(); ++i)
      leaf(prefix + "[" + std::to_string(i) + "]", j[i]);
}

void Store::ingest(const Message &msg) {
  if (!msg.payload.is_object())
    return;
  // MADS timecode: seconds since midnight UTC.
  // shortcut: no midnight wrap-around handling, fix when sessions span 00:00
  // UTC
  const auto tc = msg.payload.find("timecode");
  const double t = (tc != msg.payload.end() && tc->is_number())
                       ? tc->get<double>()
                       : msg.recv_time;
  _latest_t = std::max(_latest_t, t);
  _first_t = _messages == 0 ? t : std::min(_first_t, t);
  ++_messages;
  flatten("/" + msg.topic, msg.payload,
          [&](const std::string &path, const json &v) {
            // timecode is the time axis itself; timestamp duplicates it
            if (path == "/" + msg.topic + "/timecode" ||
                path == "/" + msg.topic + "/timestamp")
              return;
            add(path, v, t);
          });
}

void Store::add(const std::string &path, const json &value, double t) {
  Series &s = _series[path];
  if (s.last.is_null())
    s.kind = kind_of(value);
  const Kind k = kind_of(value);
  if (k == Kind::Number || k == Kind::Bool) {
    if (s.kind != Kind::Number && s.kind != Kind::Bool)
      return; // type changed under the same keypath: ignore
    s.t.push_back(t);
    s.v.push_back(k == Kind::Bool ? (value.get<bool>() ? 1.0 : 0.0)
                                  : value.get<double>());
    // amortized trimming: drop the oldest quarter once over capacity
    if (s.t.size() > _max_points + _max_points / 4) {
      const auto n = static_cast<std::ptrdiff_t>(s.t.size() - _max_points);
      s.t.erase(s.t.begin(), s.t.begin() + n);
      s.v.erase(s.v.begin(), s.v.begin() + n);
    }
  } else if (k == Kind::Array && s.kind == Kind::Array) {
    s.snaps.emplace_back(t, value.get<std::vector<double>>());
    if (s.snaps.size() > _max_snapshots)
      s.snaps.pop_front();
  } else if (k == Kind::String && s.kind == Kind::String) {
    if (s.events.empty() || s.events.back().second != value.get<std::string>())
      s.events.emplace_back(t, value.get<std::string>());
  }
  s.last = value;
  s.last_t = t;
}

void Store::clear() {
  _series.clear();
  _messages = 0;
  _latest_t = 0;
  _first_t = 0;
}

} // namespace mads_dash::data
