#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mads_dash::data {

/// One received MADS message, as handed from the network thread to the UI.
struct Message {
  std::string topic;
  nlohmann::json payload;
  double recv_time = 0; ///< UTC seconds since the Unix epoch, at reception
};

/// What a keypath holds, decided from the first value seen.
enum class Kind { Number, Bool, String, Array, Matrix, Other };

/// History of one keypath. Numbers and bools get a (t, v) series; strings get
/// change events; arrays keep their recent snapshots; matrices keep only their
/// latest value (`last`).
struct Series {
  Kind kind = Kind::Other;
  std::vector<double> t, v;                           ///< Number, Bool (0/1)
  std::vector<std::pair<double, std::string>> events; ///< String, on change
  std::deque<std::pair<double, std::vector<double>>>
      snaps; ///< Array, oldest 1st
  nlohmann::json last;
  double last_t = 0;
};

/// Where a topic's sample times come from.
struct TimeBase {
  std::string field; ///< keypath of the time field; empty = MADS `timecode`
  /// Times are absolute UTC (timecode, `$date`, epoch-like numbers), so they
  /// can be shown as clock time and compared across topics.
  bool clock = true;
};

/// All received data, keyed by keypath (`/topic/a/b`, `/topic/arr[2]`).
/// Times are UTC seconds since the Unix epoch, unless a topic uses a
/// non-clock time field. Owned and touched by the UI thread only.
class Store {
public:
  /// Hard caps per keypath (0 = none); normally retention bounds memory.
  explicit Store(std::size_t max_points = 0, std::size_t max_snapshots = 0)
      : _max_points(max_points), _max_snapshots(max_snapshots) {}

  void ingest(const Message &msg);
  void clear();

  /// Samples older than `latest_time() - seconds` are dropped by trim()
  /// (0 = keep everything).
  void set_retention(double seconds) { _retention = seconds; }
  double retention() const { return _retention; }
  void trim();

  /// Uses `keypath` (a Number, or a `.../$date` string or number) as the time
  /// of every sample of its topic; an empty `keypath` restores the timecode.
  /// The topic's data are cleared, as old and new times are not comparable.
  void set_time_field(const std::string &topic, const std::string &keypath);
  const TimeBase &time_base(const std::string &topic) const;
  /// Whether this field can be chosen with set_time_field().
  static bool can_be_time(const std::string &keypath, const Series &s);
  static std::string topic_of(const std::string &keypath);

  /// Approximate memory held by the samples, in bytes.
  std::size_t memory_bytes() const;

  const std::map<std::string, Series> &series() const { return _series; }
  double latest_time() const { return _latest_t; }
  double first_time() const { return _first_t; } ///< 0 when empty
  std::size_t messages() const { return _messages; }

private:
  void add(const std::string &path, const nlohmann::json &value, double t);

  std::map<std::string, Series> _series;
  std::size_t _max_points;
  std::size_t _max_snapshots;
  double _retention = 0;
  std::map<std::string, TimeBase> _time_bases; ///< non-default topics only
  std::size_t _messages = 0;
  double _latest_t = 0;
  double _first_t = 0;
};

/// Keypath of a field: `/topic` + `/key` per object level + `[i]` per index.
/// Calls `leaf(path, value)` for every leaf (numbers, bools, strings, numeric
/// arrays and numeric matrices are leaves; objects and other arrays recurse).
/// Numeric arrays of at most `max_elements` also yield one leaf per element.
void flatten(const std::string &prefix, const nlohmann::json &j,
             const std::function<void(const std::string &,
                                      const nlohmann::json &)> &leaf,
             std::size_t max_elements = 16);

Kind kind_of(const nlohmann::json &j);

/// MADS timecode (seconds since midnight UTC) to UTC epoch seconds, taking
/// the date from `recv_utc`; a timecode more than 12 h away from it belongs
/// to the previous or next day (messages sent around midnight).
double timecode_to_utc(double timecode, double recv_utc);

/// ISO 8601 date-time (`2024-06-01T12:34:56.789Z`, `...+02:00`, no zone = UTC)
/// to UTC epoch seconds.
std::optional<double> parse_iso8601(const std::string &s);

} // namespace mads_dash::data
