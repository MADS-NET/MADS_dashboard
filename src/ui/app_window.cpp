#include "ui/app_window.hpp"

#include "app/version.hpp"
#include "data/export_hdf5.hpp"
#include "ui/layout.hpp"
#include "ui/style.hpp"
#include "ui/views/curve_view.hpp"

#include <GLFW/glfw3.h>
#include <agent_c.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h> // DockBuilder API
#include <implot.h>
#include <implot3d.h>
#include <portable-file-dialogs.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace mads_dash::ui {

namespace {
constexpr const char *kSourcesWin = "Sources";
constexpr const char *kViewsWin = "Views";
constexpr const char *kTimelineWin = "Timeline";

void glfw_error(int code, const char *msg) {
  fprintf(stderr, "GLFW error %d: %s\n", code, msg);
}
} // namespace

AppWindow::AppWindow(Options opts) : _opts(std::move(opts)) {
  connect(_opts.receiver_args);
  glfwSetErrorCallback(glfw_error);
  if (!glfwInit())
    throw std::runtime_error("cannot initialize GLFW");

#if defined(__APPLE__)
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

  _window = glfwCreateWindow(_opts.width, _opts.height, _opts.title.c_str(),
                             nullptr, nullptr);
  if (!_window) {
    glfwTerminate();
    throw std::runtime_error("cannot create GLFW window");
  }
  glfwMakeContextCurrent(_window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImPlot3D::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr; // docking state lives in the session/layout files
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  apply_style();
  load_monospace_font();

  ImGui_ImplGlfw_InitForOpenGL(_window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Restore the last session (views, options, docking), if any.
  if (std::filesystem::exists(session_file())) {
    try {
      apply_layout(load_layout_file(session_file()));
    } catch (const std::exception &e) {
      std::fprintf(stderr, "mads-dashboard: session not restored: %s\n",
                   e.what());
    }
  }
}

AppWindow::~AppWindow() {
  try {
    save_layout_file(session_file(), current_layout());
  } catch (const std::exception &e) {
    std::fprintf(stderr, "mads-dashboard: session not saved: %s\n", e.what());
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot3D::DestroyContext();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  if (_window)
    glfwDestroyWindow(_window);
  glfwTerminate();
}

int AppWindow::run() {
  using clock = std::chrono::steady_clock;
  const auto frame_budget =
      _opts.target_fps > 0
          ? std::chrono::microseconds(1'000'000 / _opts.target_fps)
          : std::chrono::microseconds(0);

  while (!glfwWindowShouldClose(_window)) {
    // MADS traps SIGINT/SIGTERM and only clears this flag: quit cleanly, so
    // the session is saved.
    if (!mads_process_running())
      glfwSetWindowShouldClose(_window, GLFW_TRUE);
    const auto frame_start = clock::now();
    glfwPollEvents();
    if (glfwGetWindowAttrib(_window, GLFW_ICONIFIED)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    if (!_pending_ini.empty()) { // must happen outside a frame
      ImGui::LoadIniSettingsFromMemory(_pending_ini.c_str());
      _pending_ini.clear();
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    draw_frame();
    ImGui::Render();

    int w, h;
    glfwGetFramebufferSize(_window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(kClearColor.x, kClearColor.y, kClearColor.z, kClearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(_window);

    if (frame_budget.count() > 0) {
      const auto elapsed = clock::now() - frame_start;
      if (elapsed < frame_budget)
        std::this_thread::sleep_for(frame_budget - elapsed);
    }
  }
  return 0;
}

void AppWindow::connect(std::vector<std::string> args) {
  _receiver.reset(); // joins the old network thread first
  _receiver = std::make_unique<net::Receiver>(std::move(args));
}

void AppWindow::notify(std::string text, bool error) {
  _notice = std::move(text);
  _notice_error = error;
  if (error)
    std::fprintf(stderr, "mads-dashboard: %s\n", _notice.c_str());
}

nlohmann::json AppWindow::current_layout() const {
  return layout_to_json(_views, _timeline.window, _buffer,
                        ImGui::SaveIniSettingsToMemory());
}

void AppWindow::apply_layout(const nlohmann::json &j) {
  std::vector<std::string> warnings;
  Layout l = layout_from_json(j, warnings);
  for (const auto &w : warnings)
    std::fprintf(stderr, "mads-dashboard: layout: %s\n", w.c_str());
  _views = std::move(l.views);
  _next_view_id = l.next_view_id;
  _timeline.window = l.window;
  _buffer = l.buffer;
  _store.set_retention(_buffer);
  _pending_ini = l.imgui_ini;
}

void AppWindow::file_actions() {
  // Native dialogs block this (UI) thread only; the receiver keeps queueing.
  if (ImGui::MenuItem("Save layout...")) {
    auto path = pfd::save_file("Save layout", "layout.json",
                               {"Layout (*.json)", "*.json"})
                    .result();
    if (!path.empty()) {
      try {
        save_layout_file(path, current_layout());
        notify("layout saved to " + path);
      } catch (const std::exception &e) {
        notify(e.what(), true);
      }
    }
  }
  if (ImGui::MenuItem("Load layout...")) {
    auto paths =
        pfd::open_file("Load layout", "", {"Layout (*.json)", "*.json"})
            .result();
    if (!paths.empty()) {
      try {
        apply_layout(load_layout_file(paths.front()));
        notify("layout loaded from " + paths.front());
      } catch (const std::exception &e) {
        notify(e.what(), true);
      }
    }
  }
  ImGui::Separator();
  const bool any = _store.messages() > 0;
  const bool whole =
      ImGui::MenuItem("Export HDF5: whole session...", nullptr, false, any);
  const bool visible =
      ImGui::MenuItem("Export HDF5: timeline window...", nullptr, false, any);
  if (whole || visible) {
    auto path = pfd::save_file("Export HDF5", "mads_dashboard.h5",
                               {"HDF5 (*.h5 *.hdf5)", "*.h5 *.hdf5"})
                    .result();
    if (!path.empty()) {
      const double t1 = whole ? _store.latest_time() : _timeline.now;
      const double t0 = whole ? _store.first_time() : _timeline.begin();
      try {
        const auto st = data::export_hdf5(_store, path, t0, t1);
        notify("exported " + std::to_string(st.samples) + " samples to " +
               path);
      } catch (const std::exception &e) {
        notify(e.what(), true);
      }
    }
  }
}

void AppWindow::draw_frame() {
  // Network -> store, once per frame, on the UI thread (single owner).
  if (_receiver) {
    auto msgs = _receiver->drain();
    for (const auto &m : msgs)
      _store.ingest(m);
    const float dt = std::max(ImGui::GetIO().DeltaTime, 1e-3f);
    _msg_rate += 0.05 * (msgs.size() / dt - _msg_rate);
  }
  if (ImGui::GetTime() - _last_trim > 1.0) { // drop data older than the buffer
    _store.set_retention(_buffer);
    _store.trim();
    _last_trim = ImGui::GetTime();
  }
  if (!_timeline.paused)
    _timeline.now = _store.latest_time();
  _timeline.first = _store.first_time();
  if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space))
    _timeline.paused = !_timeline.paused;

  draw_menu_bar();
  const ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();
  if (!_layout_built) {
    build_default_layout(dockspace_id);
    _layout_built = true;
  }
  draw_sources();
  draw_timeline();

  ImGui::Begin(kViewsWin);
  _views_dock = ImGui::GetWindowDockID(); // new views dock here as tabs
  ImGui::TextDisabled("Right-click a field in %s to create a view.",
                      kSourcesWin);
  ImGui::End();
  draw_views();
}

void AppWindow::draw_menu_bar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Connect..."))
        _open_connect = true;
      if (ImGui::MenuItem("Clear data"))
        _store.clear();
      ImGui::Separator();
      file_actions();
      ImGui::Separator();
      if (ImGui::MenuItem("Quit"))
        glfwSetWindowShouldClose(_window, GLFW_TRUE);
      ImGui::EndMenu();
    }
    ImGui::Separator();
    using State = net::Receiver::State;
    const State st = _receiver ? _receiver->state() : State::Stopped;
    const ImVec4 col = st == State::Connected ? ImVec4(0.4f, 0.9f, 0.4f, 1)
                       : st == State::Failed  ? ImVec4(1, 0.4f, 0.4f, 1)
                                              : ImVec4(0.9f, 0.8f, 0.3f, 1);
    ImGui::TextColored(col, "%s",
                       st == State::Connected    ? "connected"
                       : st == State::Connecting ? "connecting"
                       : st == State::Failed     ? "failed"
                                                 : "disconnected");
    if (_receiver)
      ImGui::TextDisabled("%s", _receiver->status().c_str());
    ImGui::Separator();
    ImGui::Text("%.0f msg/s", _msg_rate);
    ImGui::Separator();
    ImGui::Text("data %.1f MB", _store.memory_bytes() / 1e6);
    if (_receiver && _receiver->dropped() > 0)
      ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "dropped %zu (overload)",
                         _receiver->dropped());
    ImGui::Separator();
    ImGui::TextDisabled("%.0f fps | MADS %s", ImGui::GetIO().Framerate,
                        app::mads_version());
    if (!_notice.empty()) {
      ImGui::Separator();
      if (_notice_error)
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", _notice.c_str());
      else
        ImGui::TextDisabled("%s", _notice.c_str());
    }
    ImGui::EndMainMenuBar();
  }
  draw_connect_popup();
}

void AppWindow::draw_connect_popup() {
  if (_open_connect) {
    ImGui::OpenPopup("Connect");
    _open_connect = false;
  }
  if (!ImGui::BeginPopupModal("Connect", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    return;
  ImGui::RadioButton("Broker settings URI", &_connect_mode, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Room (discovery)", &_connect_mode, 1);
  if (_connect_mode == 0)
    ImGui::InputText("URI", _uri, sizeof(_uri));
  else
    ImGui::InputText("Room", _room, sizeof(_room));
  ImGui::InputText("Topics", _topics, sizeof(_topics));
  ImGui::TextDisabled("space-separated, MQTT wildcards ok; empty = settings");
  if (ImGui::Button("Connect")) {
    std::vector<std::string> args;
    if (_connect_mode == 0)
      args = {"-s", _uri};
    else
      args = {"--room", _room};
    std::istringstream topics(_topics);
    for (std::string t; topics >> t;)
      args.push_back(t);
    connect(std::move(args));
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel"))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void AppWindow::draw_sources() {
  ImGui::Begin(kSourcesWin);
  // Keys are sorted, so all keys under one node are contiguous: walk them
  // keeping a stack of open tree nodes.
  std::vector<std::string> stack;
  std::string closed; // prefix of a collapsed node: skip keys under it
  for (const auto &[key, s] : _store.series()) {
    if (!closed.empty() && key.starts_with(closed) &&
        (key[closed.size()] == '/' || key[closed.size()] == '['))
      continue;
    std::vector<std::string> comps;
    for (std::size_t a = 1, b; a <= key.size(); a = b + 1) {
      b = std::min(key.find('/', a), key.size());
      comps.push_back(key.substr(a, b - a));
    }
    std::size_t common = 0;
    while (common < stack.size() && common + 1 < comps.size() &&
           stack[common] == comps[common])
      ++common;
    while (stack.size() > common) {
      ImGui::TreePop();
      stack.pop_back();
    }
    bool visible = true;
    std::string path;
    for (std::size_t i = 0; i < stack.size(); ++i)
      path += "/" + comps[i];
    for (std::size_t i = stack.size(); i + 1 < comps.size(); ++i) {
      path += "/" + comps[i];
      const bool open = ImGui::TreeNode(path.c_str(), "%s", comps[i].c_str());
      if (ImGui::BeginPopupContextItem(path.c_str())) {
        draw_node_menu(path);
        ImGui::EndPopup();
      }
      if (!open) {
        closed = path;
        visible = false;
        break;
      }
      stack.push_back(comps[i]);
    }
    if (!visible)
      continue;
    closed.clear();

    ImGui::TreeNodeEx(key.c_str(),
                      ImGuiTreeNodeFlags_Leaf |
                          ImGuiTreeNodeFlags_NoTreePushOnOpen |
                          ImGuiTreeNodeFlags_SpanAvailWidth,
                      "%s", comps.back().c_str());
    if (ImGui::BeginPopupContextItem(key.c_str())) {
      draw_source_menu(key, s);
      ImGui::EndPopup();
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s\n%s", key.c_str(),
                        s.last.dump().substr(0, 200).c_str());
    ImGui::SameLine();
    if (s.kind == data::Kind::Number || s.kind == data::Kind::Bool ||
        s.kind == data::Kind::String)
      ImGui::TextDisabled("%s", s.last.dump().c_str());
    else if (s.kind == data::Kind::Array)
      ImGui::TextDisabled("[%zu]", s.last.size());
    else if (s.kind == data::Kind::Matrix)
      ImGui::TextDisabled("[%zux%zu]", s.last.size(), s.last.front().size());
  }
  while (!stack.empty()) {
    ImGui::TreePop();
    stack.pop_back();
  }
  if (_pending_time) { // safe now: the tree is no longer iterating
    _store.set_time_field(_pending_time->first, _pending_time->second);
    _pending_time.reset();
  }
  if (_store.series().empty())
    ImGui::TextDisabled("(no data yet)");
  ImGui::End();
}

void AppWindow::draw_node_menu(const std::string &path) {
  const std::string topic = data::Store::topic_of(path);
  const auto &tb = _store.time_base(topic);
  if (path == "/" + topic) {
    ImGui::TextDisabled("time: %s",
                        tb.field.empty() ? "MADS timecode" : tb.field.c_str());
    if (!tb.field.empty() && ImGui::MenuItem("Use MADS timecode as time"))
      _pending_time = {topic, ""};
  } else {
    ImGui::TextDisabled("%s", path.c_str());
  }
  std::string xk, yk;
  if (curve_pair(_store, path, xk, yk) &&
      ImGui::MenuItem("New XY curve view")) {
    for (const auto &type : view_types())
      if (std::string(type.name) == "XY curve") {
        auto v = type.make(_next_view_id++);
        v->add_source(xk);
        v->add_source(yk);
        _views.push_back(std::move(v));
      }
  }
}

void AppWindow::draw_source_menu(const std::string &key,
                                 const data::Series &s) {
  const std::string topic = data::Store::topic_of(key);
  if (data::Store::can_be_time(key, s)) {
    if (ImGui::MenuItem(("Use as time of " + topic).c_str()))
      _pending_time = {topic, key};
    ImGui::SetItemTooltip("Times of every sample of this topic come from "
                          "this field; its received data are cleared");
    ImGui::Separator();
  }
  bool any = false;
  for (const auto &type : view_types()) {
    if (!type.accepts(s.kind))
      continue;
    any = true;
    if (ImGui::MenuItem((std::string("New ") + type.name + " view").c_str())) {
      auto v = type.make(_next_view_id++);
      v->add_source(key);
      _views.push_back(std::move(v));
    }
  }
  if (!any)
    ImGui::TextDisabled("No view for this kind of data");
  for (auto &v : _views)
    if (v->accepts(s.kind) &&
        ImGui::MenuItem(
            ("Add to " + v->title().substr(0, v->title().find("###"))).c_str()))
      v->add_source(key);
}

void AppWindow::draw_timeline() {
  ImGui::Begin(kTimelineWin);
  if (ImGui::Button(_timeline.paused ? "Resume (space)" : "Pause (space)"))
    _timeline.paused = !_timeline.paused;

  // Common window: 1-2-5 steps up to 1000 s, then "all" (0).
  static constexpr double kSteps[] = {1,   2,   5,   10,   20, 50,
                                      100, 200, 500, 1000, 0};
  constexpr int kAll = int(std::size(kSteps)) - 1;
  int idx = kAll;
  for (int i = 0; i < kAll; ++i)
    if (_timeline.window > 0 && _timeline.window <= kSteps[i] * 1.0001) {
      idx = i;
      break;
    }
  char label[32];
  if (idx == kAll)
    std::snprintf(label, sizeof label, "all");
  else
    std::snprintf(label, sizeof label, "%g s", kSteps[idx]);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(160);
  if (ImGui::SliderInt("window", &idx, 0, kAll, label,
                       ImGuiSliderFlags_NoInput))
    _timeline.window = kSteps[idx];
  ImGui::SetItemTooltip("Data shown or used by every view: time-series span, "
                        "trace length, histogram/boxplot/spectrum input");

  ImGui::SameLine();
  ImGui::SetNextItemWidth(90);
  if (ImGui::InputDouble("buffer [s]", &_buffer, 0, 0, "%.0f"))
    _buffer = std::max(0.0, _buffer);
  ImGui::SetItemTooltip("Data older than this are dropped (0 = keep all)");

  ImGui::SameLine();
  const double day = std::fmod(_timeline.now, 86400.0);
  const int t = (int)day;
  ImGui::Text("  %02d:%02d:%02d.%03d UTC  %s", t / 3600, t / 60 % 60, t % 60,
              (int)((day - t) * 1000),
              _timeline.paused ? "(paused)" : "(live)");
  ImGui::Text("%zu messages, %zu keypaths. Double-click a plot: autoscale; "
              "drag or zoom: manual.",
              _store.messages(), _store.series().size());
  // Review: while paused, scrub `now` over everything buffered.
  ImGui::BeginDisabled(!_timeline.paused || _store.messages() == 0);
  ImGui::SetNextItemWidth(-FLT_MIN);
  double now = _timeline.now;
  const double t0 = _store.first_time(), t1 = _store.latest_time();
  char scrub[32];
  const int st = (int)std::fmod(now, 86400.0);
  std::snprintf(scrub, sizeof scrub, "%02d:%02d:%02d", st / 3600, st / 60 % 60,
                st % 60);
  if (ImGui::SliderScalar("##scrub", ImGuiDataType_Double, &now, &t0, &t1,
                          scrub))
    _timeline.now = now;
  ImGui::EndDisabled();
  ImGui::End();
}

void AppWindow::draw_views() {
  for (auto it = _views.begin(); it != _views.end();) {
    View &v = **it;
    bool open = true;
    ImGui::SetNextWindowDockID(_views_dock, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(v.title().c_str(), &open, ImGuiWindowFlags_MenuBar)) {
      if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Sources")) {
          std::string remove;
          for (const auto &k : v.sources())
            if (ImGui::MenuItem(("Remove " + k).c_str()))
              remove = k;
          if (!remove.empty())
            v.remove_source(remove);
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options")) {
          v.draw_options();
          ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
      }
      v.draw(_store, _timeline);
    }
    ImGui::End();
    it = open ? it + 1 : _views.erase(it);
  }
}

void AppWindow::build_default_layout(unsigned int dockspace_id) {
  // Respect a layout restored from imgui.ini.
  if (ImGui::DockBuilderGetNode(dockspace_id) &&
      ImGui::DockBuilderGetNode(dockspace_id)->IsSplitNode())
    return;

  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id,
                                ImGui::GetMainViewport()->WorkSize);

  ImGuiID center = dockspace_id;
  ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.18f,
                                               nullptr, &center);
  ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f,
                                             nullptr, &center);

  ImGui::DockBuilderDockWindow(kSourcesWin, left);
  ImGui::DockBuilderDockWindow(kTimelineWin, bottom);
  ImGui::DockBuilderDockWindow(kViewsWin, center);
  ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace mads_dash::ui
