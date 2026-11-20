#include "ui/view.hpp"
#include "ui/views/time_series_view.hpp"
#include "ui/views/xy_view.hpp"

#include <algorithm>
#include <vector>

namespace mads_dash::ui {

const std::vector<ViewType> &view_types() {
  static const std::vector<ViewType> types{
      {"Time series",
       [](int id) { return std::make_unique<TimeSeriesView>(id); }},
      {"XY", [](int id) { return std::make_unique<XYView>(id); }},
  };
  return types;
}

void View::add_source(const std::string &keypath) {
  if (std::find(_sources.begin(), _sources.end(), keypath) == _sources.end())
    _sources.push_back(keypath);
}

void View::remove_source(const std::string &keypath) {
  std::erase(_sources, keypath);
}

std::string View::title() const {
  return std::string(type_name()) + " #" + std::to_string(_id) + "###view" +
         std::to_string(_id);
}

} // namespace mads_dash::ui
