#pragma once

#include "data/store.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mads_dash::ui {

/// Shared time state: one timeline drives every view.
struct Timeline {
  bool paused = false;
  /// Seconds of data shown or used by every view (x span of time series,
  /// trace length, histogram/boxplot/spectrum input); 0 = all buffered data.
  double window = 10.0;
  double now = 0;   ///< right edge: latest time (live) or the scrubbed time
  double first = 0; ///< oldest buffered time
  /// Left edge of the window.
  double begin() const { return window > 0 ? now - window : first; }
};

/// A chart showing one or more keypaths. Each view is its own dockable window.
/// M2 adds the other chart types as subclasses of this interface.
class View {
public:
  explicit View(int id) : _id(id) {}
  virtual ~View() = default;

  virtual const char *type_name() const = 0;
  /// Whether this view can show a keypath of this kind.
  virtual bool accepts(data::Kind kind) const = 0;
  /// Draws the plot contents inside the already-open view window.
  virtual void draw(const data::Store &store, const Timeline &tl) = 0;
  /// Optional per-view settings, drawn inside the window's "Options" menu.
  virtual void draw_options() {}
  /// The settings of draw_options() as JSON, for layout files; set_options()
  /// restores them, ignoring missing or mistyped keys.
  virtual nlohmann::json options() const { return nlohmann::json::object(); }
  virtual void set_options(const nlohmann::json &) {}

  void add_source(const std::string &keypath);
  void remove_source(const std::string &keypath);
  const std::vector<std::string> &sources() const { return _sources; }
  int id() const { return _id; }
  std::string title() const; ///< "<type> #<id>###view<id>"

protected:
  std::vector<std::string> _sources;
  int _id;
};

/// A chart type that can be created from the source tree.
struct ViewType {
  const char *name;
  std::function<std::unique_ptr<View>(int id)> make;
  bool accepts(data::Kind k) const { return make(0)->accepts(k); }
};

/// All chart types, in menu order. Register a new view type in view.cpp.
const std::vector<ViewType> &view_types();

} // namespace mads_dash::ui
