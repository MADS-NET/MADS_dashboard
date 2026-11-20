#pragma once

#include "ui/view.hpp"

#include <string>

namespace mads_dash::ui {

/// Latest value of `s` at or before `now`, formatted for display: numbers with
/// 6 significant digits, bools as true/false, strings as is, arrays as
/// "[a, b, c]" (at most 8 elements, then "..."). "-" if there is none.
std::string readout_text(const data::Series &s, double now);

/// Table with one row per source: keypath, latest value, age in seconds.
class ReadoutView : public View {
public:
  using View::View;
  const char *type_name() const override { return "Readout"; }
  bool accepts(data::Kind k) const override {
    return k == data::Kind::Number || k == data::Kind::Bool ||
           k == data::Kind::String || k == data::Kind::Array;
  }
  void draw(const data::Store &store, const Timeline &tl) override;
  void draw_options() override;
  nlohmann::json options() const override;
  void set_options(const nlohmann::json &j) override;

private:
  bool _big = true;
};

} // namespace mads_dash::ui
