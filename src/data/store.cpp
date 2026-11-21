#include "data/store.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

double timecode_to_utc(double timecode, double recv_utc) {
  constexpr double day = 86400, half = 43200;
  double t = std::floor(recv_utc / day) * day + timecode;
  if (t - recv_utc > half)
    t -= day;
  else if (recv_utc - t > half)
    t += day;
  return t;
}

std::optional<double> parse_iso8601(const std::string &s) {
  int Y, M, D, h, m;
  double sec;
  char sep;
  int n = 0;
  if (std::sscanf(s.c_str(), "%4d-%2d-%2d%c%2d:%2d:%lf%n", &Y, &M, &D, &sep, &h,
                  &m, &sec, &n) != 7 ||
      (sep != 'T' && sep != 't' && sep != ' '))
    return std::nullopt;
  // days from civil (H. Hinnant), proleptic Gregorian calendar
  const int y = Y - (M <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const double days = double(era) * 146097 + double(doe) - 719468;
  double t = days * 86400 + h * 3600 + m * 60 + sec;
  const char *zone = s.c_str() + n;
  if (*zone == '+' || *zone == '-') {
    int zh = 0, zm = 0;
    if (std::sscanf(zone + 1, "%2d:%2d", &zh, &zm) < 1 &&
        std::sscanf(zone + 1, "%2d%2d", &zh, &zm) < 1)
      return std::nullopt;
    t -= (*zone == '+' ? 1 : -1) * (zh * 3600 + zm * 60);
  }
  return t;
}

namespace {
const TimeBase kDefaultTimeBase{};

/// Value at `keypath` (`/topic/a/b`) inside a message of that topic.
const json *lookup(const json &payload, const std::string &topic,
                   const std::string &keypath) {
  const json *j = &payload;
  for (std::size_t a = topic.size() + 2, b; a <= keypath.size(); a = b + 1) {
    b = std::min(keypath.find('/', a), keypath.size());
    if (!j->is_object())
      return nullptr;
    auto it = j->find(keypath.substr(a, b - a));
    if (it == j->end())
      return nullptr;
    j = &*it;
  }
  return j;
}

std::optional<double> time_value(const json &v, bool is_date) {
  if (is_date && v.is_string())
    return parse_iso8601(v.get<std::string>());
  if (is_date && v.is_number())
    return v.get<double>() / 1000; // extended JSON: ms since the epoch
  if (!is_date && v.is_number())
    return v.get<double>();
  return std::nullopt;
}

bool is_date_key(const std::string &keypath) {
  return keypath.size() > 6 && keypath.ends_with("/$date");
}
} // namespace

std::string Store::topic_of(const std::string &keypath) {
  return keypath.substr(1, keypath.find('/', 1) - 1);
}

bool Store::can_be_time(const std::string &keypath, const Series &s) {
  return s.kind == Kind::Number ||
         (is_date_key(keypath) && (s.kind == Kind::String));
}

const TimeBase &Store::time_base(const std::string &topic) const {
  auto it = _time_bases.find(topic);
  return it == _time_bases.end() ? kDefaultTimeBase : it->second;
}

void Store::set_time_field(const std::string &topic,
                           const std::string &keypath) {
  if (keypath.empty())
    _time_bases.erase(topic);
  else
    _time_bases[topic] = {keypath, true};
  const std::string prefix = "/" + topic;
  std::erase_if(_series, [&](const auto &kv) {
    const std::string &k = kv.first;
    return k.starts_with(prefix) &&
           (k.size() == prefix.size() || k[prefix.size()] == '/' ||
            k[prefix.size()] == '[');
  });
}

void Store::ingest(const Message &msg) {
  if (!msg.payload.is_object())
    return;
  const std::string prefix = "/" + msg.topic;
  auto tb_it = _time_bases.find(msg.topic);
  double t;
  std::string skip; // the time field itself is the axis, not a series
  if (tb_it == _time_bases.end()) {
    const auto tc = msg.payload.find("timecode");
    t = (tc != msg.payload.end() && tc->is_number())
            ? timecode_to_utc(tc->get<double>(), msg.recv_time)
            : msg.recv_time;
    skip = prefix + "/timecode";
  } else {
    TimeBase &tb = tb_it->second;
    const json *v = lookup(msg.payload, msg.topic, tb.field);
    const bool date = is_date_key(tb.field);
    const auto tv = v ? time_value(*v, date) : std::nullopt;
    if (!tv)
      return; // no usable time in this message
    t = *tv;
    tb.clock = date || t > 1e8; // epoch-like numbers (after 1973) are clock
    skip = tb.field;
  }
  _latest_t = _messages == 0 ? t : std::max(_latest_t, t);
  _first_t = _messages == 0 ? t : std::min(_first_t, t);
  ++_messages;
  flatten(prefix, msg.payload, [&](const std::string &path, const json &v) {
    // timestamp duplicates the timecode as an ISO string
    if (path == skip || path == prefix + "/timestamp")
      return;
    if (is_date_key(path)) { // a date is a time value, not a state: number
      if (const auto d = time_value(v, true))
        add(path, json(*d), t);
      return;
    }
    add(path, v, t);
  });
}

void Store::trim() {
  if (_retention <= 0 || _messages == 0)
    return;
  const double cutoff = _latest_t - _retention;
  for (auto &[key, s] : _series) {
    const auto n =
        std::lower_bound(s.t.begin(), s.t.end(), cutoff) - s.t.begin();
    s.t.erase(s.t.begin(), s.t.begin() + n);
    s.v.erase(s.v.begin(), s.v.begin() + n);
    std::erase_if(s.events, [&](const auto &e) { return e.first < cutoff; });
    while (!s.snaps.empty() && s.snaps.front().first < cutoff)
      s.snaps.pop_front();
  }
  _first_t = std::max(_first_t, cutoff);
}

std::size_t Store::memory_bytes() const {
  std::size_t n = 0;
  for (const auto &[key, s] : _series) {
    n += sizeof(Series) + key.capacity() + 64; // map node
    n += (s.t.capacity() + s.v.capacity()) * sizeof(double);
    for (const auto &e : s.events)
      n += sizeof(e) + e.second.capacity();
    for (const auto &[t, a] : s.snaps)
      n += sizeof(t) + sizeof(a) + a.capacity() * sizeof(double);
    if (s.kind == Kind::Matrix && s.last.is_array() && !s.last.empty())
      n += s.last.size() * s.last.front().size() * 16; // json numbers
  }
  return n;
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
    // amortized cap: drop the oldest quarter once over capacity
    if (_max_points > 0 && s.t.size() > _max_points + _max_points / 4) {
      const auto n = static_cast<std::ptrdiff_t>(s.t.size() - _max_points);
      s.t.erase(s.t.begin(), s.t.begin() + n);
      s.v.erase(s.v.begin(), s.v.begin() + n);
    }
  } else if (k == Kind::Array && s.kind == Kind::Array) {
    s.snaps.emplace_back(t, value.get<std::vector<double>>());
    if (_max_snapshots > 0 && s.snaps.size() > _max_snapshots)
      s.snaps.pop_front();
  } else if (k == Kind::String && s.kind == Kind::String) {
    if (s.events.empty() || s.events.back().second != value.get<std::string>())
      s.events.emplace_back(t, value.get<std::string>());
  }
  s.last = value;
  s.last_t = t;
}

void Store::clear() {
  _series.clear(); // time bases are settings: kept
  _messages = 0;
  _latest_t = 0;
  _first_t = 0;
}

} // namespace mads_dash::data
