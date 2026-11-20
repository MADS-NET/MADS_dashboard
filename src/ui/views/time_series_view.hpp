#pragma once

#include "ui/view.hpp"

namespace mads_dash::ui {

/// Strip chart of numbers and bools over the timeline window; strings are
/// drawn as labelled event lines.
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
