#pragma once

#include "data/store.hpp"

#include <string>
#include <vector>

namespace mads_dash::ui {

/// Shared time state: one timeline drives every view (PLAN §8).
struct Timeline {
  bool paused = false;
  double window = 10.0; ///< seconds shown in live mode
  double now = 0;       ///< right edge of the live window
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

  void add_source(const std::string &keypath);
  void remove_source(const std::string &keypath);
  const std::vector<std::string> &sources() const { return _sources; }
  int id() const { return _id; }
  std::string title() const; ///< "<type> #<id>###view<id>"

protected:
  std::vector<std::string> _sources;
  int _id;
};

class TimeSeriesView : public View {
public:
  using View::View;
  const char *type_name() const override { return "Time series"; }
  bool accepts(data::Kind k) const override {
    return k == data::Kind::Number || k == data::Kind::Bool ||
           k == data::Kind::String;
  }
  void draw(const data::Store &store, const Timeline &tl) override;
};

} // namespace mads_dash::ui
