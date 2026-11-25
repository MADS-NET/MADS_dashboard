#include "script/script_host.hpp"

#include "ui/layout.hpp"
#include "ui/views/spectrum_view.hpp"

#include <topic_match.hpp>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using nlohmann::json;

namespace mads_dash::script {
namespace {

constexpr int kHookEvery = 1000;
constexpr std::intptr_t kBudget = 1000000;
constexpr int kMaxDepth = 32;

// Lua errors longjmp over C++ frames, skipping destructors: to_json() never
// raises (it holds json/string locals). push_json() holds none.
void push_json(lua_State *L, const json &j) {
  luaL_checkstack(L, 4, "JSON too deep");
  if (j.is_object()) {
    lua_createtable(L, 0, static_cast<int>(j.size()));
    for (auto it = j.begin(); it != j.end(); ++it) {
      push_json(L, it.value());
      lua_setfield(L, -2, it.key().c_str());
    }
  } else if (j.is_array()) {
    lua_createtable(L, static_cast<int>(j.size()), 0);
    lua_Integer i = 1;
    for (const auto &e : j) {
      push_json(L, e);
      lua_rawseti(L, -2, i++);
    }
  } else if (j.is_boolean()) {
    lua_pushboolean(L, j.get<bool>());
  } else if (j.is_number_integer()) {
    lua_pushinteger(L, j.get<std::int64_t>());
  } else if (j.is_number()) {
    lua_pushnumber(L, j.get<double>());
  } else if (j.is_string()) {
    const auto &s = j.get_ref<const std::string &>();
    lua_pushlstring(L, s.data(), s.size());
  } else {
    lua_pushnil(L);
  }
}

json to_json(lua_State *L, int i, int depth = 0) {
  switch (lua_type(L, i)) {
  case LUA_TBOOLEAN:
    return lua_toboolean(L, i) != 0;
  case LUA_TNUMBER:
    return lua_isinteger(L, i) ? json(lua_tointeger(L, i))
                               : json(lua_tonumber(L, i));
  case LUA_TSTRING: {
    std::size_t n;
    const char *s = lua_tolstring(L, i, &n);
    return std::string(s, n);
  }
  case LUA_TTABLE:
    break;
  default:
    return nullptr;
  }
  if (depth > kMaxDepth || !lua_checkstack(L, 5))
    return nullptr; // too deep (a cycle?): dropped, as the Store skips null
  i = lua_absindex(L, i);
  const auto n = static_cast<lua_Integer>(lua_rawlen(L, i));
  std::size_t keys = 0;
  for (lua_pushnil(L); lua_next(L, i); lua_pop(L, 1))
    ++keys;
  if (static_cast<std::size_t>(n) == keys) { // sequence (or empty)
    json a = json::array();
    for (lua_Integer k = 1; k <= n; ++k) {
      lua_rawgeti(L, i, k);
      a.push_back(to_json(L, -1, depth + 1));
      lua_pop(L, 1);
    }
    return a;
  }
  json o = json::object();
  for (lua_pushnil(L); lua_next(L, i); lua_pop(L, 1)) {
    const int kt = lua_type(L, -2);
    if (kt != LUA_TSTRING && kt != LUA_TNUMBER)
      continue;
    lua_pushvalue(L, -2); // a copy: tolstring would confuse lua_next
    std::size_t len;
    const char *s = lua_tolstring(L, -1, &len);
    const std::string key(s, len);
    lua_pop(L, 1);
    o[key] = to_json(L, -1, depth + 1);
  }
  return o;
}

struct Call {
  const data::Message *msg;
  double t;
  json *out;
  bool *has;
};

int run_process(lua_State *L) {
  auto *c = static_cast<Call *>(lua_touserdata(L, 1));
  lua_getglobal(L, "process");
  lua_pushlstring(L, c->msg->topic.data(), c->msg->topic.size());
  push_json(L, c->msg->payload);
  lua_pushnumber(L, c->t);
  lua_call(L, 3, 1);
  switch (lua_type(L, -1)) {
  case LUA_TNIL:
    return 0;
  case LUA_TTABLE:
    *c->out = to_json(L, -1);
    break;
  default:
    *c->out = json{{"value", to_json(L, -1)}};
  }
  *c->has = true;
  return 0;
}

// Instruction budget: a runaway script must not freeze the UI thread.
void budget_hook(lua_State *L, lua_Debug *) {
  auto &n = *static_cast<std::intptr_t *>(lua_getextraspace(L));
  n += kHookEvery;
  if (n > kBudget)
    luaL_error(L, "instruction budget exceeded");
}

// Pops the error message of a failed pcall.
std::string pop_error(lua_State *L) {
  const char *m = lua_tostring(L, -1);
  std::string s = m ? m : "error object is not a string";
  lua_pop(L, 1);
  return s;
}

int pcall_budget(lua_State *L, int nargs) {
  *static_cast<std::intptr_t *>(lua_getextraspace(L)) = 0;
  return lua_pcall(L, nargs, 0, 0);
}

int os_clock(lua_State *L) {
  lua_pushnumber(L, static_cast<double>(std::clock()) / CLOCKS_PER_SEC);
  return 1;
}

int os_time(lua_State *L) {
  lua_pushinteger(L, static_cast<lua_Integer>(std::time(nullptr)));
  return 1;
}

int dash_fft(lua_State *L) {
  for (int a = 1; a <= 2; ++a) {
    luaL_checktype(L, a, LUA_TTABLE);
    for (lua_Integer k = 1; k <= static_cast<lua_Integer>(lua_rawlen(L, a));
         ++k) {
      lua_rawgeti(L, a, k);
      if (lua_type(L, -1) != LUA_TNUMBER)
        luaL_argerror(L, a, "numbers expected");
      lua_pop(L, 1);
    }
  }
  if (lua_rawlen(L, 1) != lua_rawlen(L, 2))
    return luaL_error(L, "t and v must have the same length");
  ui::Spectrum s;
  {
    std::vector<double> t, v;
    for (int a = 1; a <= 2; ++a) {
      auto &dst = a == 1 ? t : v;
      for (lua_Integer k = 1; k <= static_cast<lua_Integer>(lua_rawlen(L, a));
           ++k) {
        lua_rawgeti(L, a, k);
        dst.push_back(lua_tonumber(L, -1));
        lua_pop(L, 1);
      }
    }
    s = ui::spectrum(t, v);
  }
  lua_createtable(L, 0, 2);
  for (const auto *col : {&s.f, &s.amp}) {
    lua_createtable(L, static_cast<int>(col->size()), 0);
    for (std::size_t k = 0; k < col->size(); ++k) {
      lua_pushnumber(L, (*col)[k]);
      lua_rawseti(L, -2, static_cast<lua_Integer>(k + 1));
    }
    lua_setfield(L, -2, col == &s.f ? "f" : "amp");
  }
  return 1;
}

void open_libs(lua_State *L) {
  const luaL_Reg libs[] = {{LUA_GNAME, luaopen_base},
                           {LUA_MATHLIBNAME, luaopen_math},
                           {LUA_STRLIBNAME, luaopen_string},
                           {LUA_TABLIBNAME, luaopen_table}};
  for (const auto &l : libs) {
    luaL_requiref(L, l.name, l.func, 1);
    lua_pop(L, 1);
  }
  for (const char *f : {"dofile", "loadfile"}) {
    lua_pushnil(L);
    lua_setglobal(L, f);
  }
  const luaL_Reg os[] = {
      {"clock", os_clock}, {"time", os_time}, {nullptr, nullptr}};
  luaL_newlib(L, os);
  lua_setglobal(L, "os");
  const luaL_Reg dash[] = {{"fft", dash_fft}, {nullptr, nullptr}};
  luaL_newlib(L, dash);
  lua_setglobal(L, "dash");
}

// Compiles and runs the chunk, reads `topics`; returns an error or "".
std::string init(lua_State *L, const std::string &name,
                 const std::string &source, std::vector<std::string> &topics) {
  const std::string file = name + ".lua";
  open_libs(L);
  lua_sethook(L, budget_hook, LUA_MASKCOUNT, kHookEvery);
  if (luaL_loadbuffer(L, source.data(), source.size(), ("@" + file).c_str()) ||
      pcall_budget(L, 0))
    return pop_error(L);
  lua_getglobal(L, "process");
  const bool has_process = lua_isfunction(L, -1);
  lua_pop(L, 1);
  if (!has_process)
    return file + ": no process() function";
  lua_getglobal(L, "topics");
  if (lua_istable(L, -1))
    for (lua_Integer k = 1; k <= static_cast<lua_Integer>(lua_rawlen(L, -1));
         ++k) {
      lua_rawgeti(L, -1, k);
      if (lua_type(L, -1) == LUA_TSTRING)
        topics.push_back(lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  lua_pop(L, 1);
  return {};
}

} // namespace

Script *ScriptHost::find(const std::string &name) {
  auto it = std::find_if(_scripts.begin(), _scripts.end(),
                         [&](const Script &s) { return s.name == name; });
  return it == _scripts.end() ? nullptr : &*it;
}

void ScriptHost::load(const std::string &name, const std::string &source,
                      const fs::path &path) {
  Script s;
  s.name = name;
  s.path = path;
  if (const Script *old = find(name))
    s.enabled = old->enabled;
  std::shared_ptr<lua_State> L(luaL_newstate(), lua_close);
  s.error = init(L.get(), name, source, s.topics);
  if (s.error.empty())
    s.L = L;
  if (Script *old = find(name)) {
    *old = std::move(s);
  } else {
    _scripts.push_back(std::move(s));
    std::sort(_scripts.begin(), _scripts.end(),
              [](const Script &a, const Script &b) { return a.name < b.name; });
  }
}

void ScriptHost::scan(const fs::path &dir) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec))
    return;
  std::vector<std::string> seen;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end;
       it.increment(ec)) {
    std::error_code e;
    const fs::path p = it->path();
    if (p.extension() != ".lua" || !it->is_regular_file(e))
      continue;
    const std::string name = p.stem().string();
    seen.push_back(name);
    const auto mtime = fs::last_write_time(p, e);
    if (e)
      continue;
    const Script *s = find(name);
    if (s && s->path == p && s->mtime == mtime)
      continue;
    std::ifstream in(p, std::ios::binary);
    std::ostringstream src;
    src << in.rdbuf();
    load(name, src.str(), p);
    find(name)->mtime = mtime;
  }
  std::erase_if(_scripts, [&](const Script &s) {
    return !s.path.empty() &&
           std::find(seen.begin(), seen.end(), s.name) == seen.end();
  });
}

void ScriptHost::on_message(const data::Message &msg, double t,
                            std::vector<data::Message> &out) {
  if (msg.topic.starts_with('~'))
    return;
  for (auto &s : _scripts) {
    if (!s.enabled || !s.L ||
        std::none_of(s.topics.begin(), s.topics.end(), [&](const auto &e) {
          return Mads::subscription_matches(e, msg.topic);
        }))
      continue;
    lua_State *L = s.L.get();
    json payload;
    bool has = false;
    Call call{&msg, t, &payload, &has};
    const auto t0 = std::chrono::steady_clock::now();
    lua_pushcfunction(L, run_process);
    lua_pushlightuserdata(L, &call);
    const int rc = pcall_budget(L, 1);
    ++s.calls;
    s.total_us += std::chrono::duration<double, std::micro>(
                      std::chrono::steady_clock::now() - t0)
                      .count();
    if (rc) {
      s.error = pop_error(L);
      s.enabled = false;
    } else if (has) {
      out.push_back({"~" + s.name, std::move(payload), msg.recv_time});
    }
  }
}

void ScriptHost::set_enabled(const std::string &name, bool on) {
  Script *s = find(name);
  if (!s || (on && !s->L))
    return;
  s->enabled = on;
  if (on)
    s->error.clear();
}

std::vector<std::string> ScriptHost::enabled() const {
  std::vector<std::string> v;
  for (const auto &s : _scripts)
    if (s.enabled)
      v.push_back(s.name);
  return v;
}

const std::vector<std::pair<const char *, const char *>> &example_scripts() {
  static const std::vector<std::pair<const char *, const char *>> v = {
#include "script/examples.inc"
  };
  return v;
}

fs::path scripts_dir() {
  const fs::path d = ui::session_file().parent_path() / "scripts";
  std::error_code ec;
  if (!fs::exists(d, ec)) {
    fs::create_directories(d, ec);
    for (const auto &[name, code] : example_scripts())
      std::ofstream(d / name, std::ios::binary) << code;
  }
  return d;
}

} // namespace mads_dash::script
