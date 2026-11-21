#include "ui/layout.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mads_dash::ui {

namespace fs = std::filesystem;
using nlohmann::json;

json layout_to_json(const std::vector<std::unique_ptr<View>> &views,
                    double window, double buffer,
                    const std::string &imgui_ini) {
  json vs = json::array();
  for (const auto &v : views)
    vs.push_back({{"type", v->type_name()},
                  {"id", v->id()},
                  {"sources", v->sources()},
                  {"options", v->options()}});
  return {{"format", 1},
          {"window", window},
          {"buffer", buffer},
          {"imgui_ini", imgui_ini},
          {"views", vs}};
}

Layout layout_from_json(const json &j, std::vector<std::string> &warnings) {
  Layout out;
  if (!j.is_object()) {
    warnings.push_back("layout is not a JSON object");
    return out;
  }
  if (auto it = j.find("window"); it != j.end() && it->is_number())
    out.window = it->get<double>();
  if (auto it = j.find("buffer"); it != j.end() && it->is_number())
    out.buffer = std::max(0.0, it->get<double>());
  if (auto it = j.find("imgui_ini"); it != j.end() && it->is_string())
    out.imgui_ini = it->get<std::string>();
  auto vs = j.find("views");
  if (vs == j.end() || !vs->is_array()) {
    warnings.push_back("layout has no \"views\" array");
    return out;
  }
  int index = 0;
  for (const auto &e : *vs) {
    const std::string where = "view entry " + std::to_string(index++);
    if (!e.is_object()) {
      warnings.push_back(where + ": not an object, skipped");
      continue;
    }
    auto type = e.find("type");
    auto id = e.find("id");
    if (type == e.end() || !type->is_string() || id == e.end() ||
        !id->is_number_integer()) {
      warnings.push_back(where +
                         ": missing or invalid \"type\"/\"id\", skipped");
      continue;
    }
    const auto &types = view_types();
    auto vt = std::find_if(types.begin(), types.end(), [&](const ViewType &t) {
      return type->get<std::string>() == t.name;
    });
    if (vt == types.end()) {
      warnings.push_back(where + ": unknown view type \"" +
                         type->get<std::string>() + "\", skipped");
      continue;
    }
    auto view = vt->make(id->get<int>());
    if (auto src = e.find("sources"); src != e.end() && src->is_array())
      for (const auto &s : *src) {
        if (s.is_string())
          view->add_source(s.get<std::string>());
        else
          warnings.push_back(where + ": non-string source ignored");
      }
    if (auto o = e.find("options"); o != e.end() && o->is_object())
      view->set_options(*o);
    out.next_view_id = std::max(out.next_view_id, view->id() + 1);
    out.views.push_back(std::move(view));
  }
  return out;
}

void save_layout_file(const fs::path &p, const json &j) {
  std::error_code ec;
  if (p.has_parent_path())
    fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f)
    throw std::runtime_error("cannot write layout file " + p.string());
  f << j.dump(2) << '\n';
  f.flush();
  if (!f)
    throw std::runtime_error("error writing layout file " + p.string());
}

json load_layout_file(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw std::runtime_error("cannot read layout file " + p.string());
  std::stringstream ss;
  ss << f.rdbuf();
  try {
    return json::parse(ss.str());
  } catch (const json::exception &e) {
    throw std::runtime_error("invalid layout file " + p.string() + ": " +
                             e.what());
  }
}

fs::path session_file() {
  auto env = [](const char *n) -> std::string {
    const char *v = std::getenv(n);
    return v ? v : "";
  };
  fs::path base;
#if defined(_WIN32)
  base = env("APPDATA");
#elif defined(__APPLE__)
  base = fs::path(env("HOME")) / "Library" / "Application Support";
#else
  if (!env("XDG_CONFIG_HOME").empty())
    base = env("XDG_CONFIG_HOME");
  else
    base = fs::path(env("HOME")) / ".config";
#endif
  return base / "mads-dashboard" / "session.json";
}

} // namespace mads_dash::ui
