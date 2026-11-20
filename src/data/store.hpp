#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mads_dash::data {

/// One received MADS message, as handed from the network thread to the UI.
struct Message {
  std::string topic;
  nlohmann::json payload;
  double recv_time = 0; ///< seconds since midnight UTC, at reception
};

/// What a keypath holds, decided from the first value seen.
enum class Kind { Number, Bool, String, Array, Matrix, Other };

/// History of one keypath. Numbers and bools get a (t, v) series; strings get
/// change events; arrays and matrices keep only their latest value (M1).
struct Series {
  Kind kind = Kind::Other;
  std::vector<double> t, v;                           ///< Number, Bool (0/1)
  std::vector<std::pair<double, std::string>> events; ///< String, on change
  nlohmann::json last;
  double last_t = 0;
};

/// All received data, keyed by keypath (`/topic/a/b`, `/topic/arr[2]`).
/// Owned and touched by the UI thread only.
class Store {
public:
  explicit Store(std::size_t max_points = 200'000) : _max_points(max_points) {}

  void ingest(const Message &msg);
  void clear();

  const std::map<std::string, Series> &series() const { return _series; }
  double latest_time() const { return _latest_t; }
  std::size_t messages() const { return _messages; }

private:
  void add(const std::string &path, const nlohmann::json &value, double t);

  std::map<std::string, Series> _series;
  std::size_t _max_points;
  std::size_t _messages = 0;
  double _latest_t = 0;
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

} // namespace mads_dash::data
