#include "ui/app_window.hpp"

#include "app/version.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h> // DockBuilder API
#include <implot.h>
#include <implot3d.h>

#include <algorithm>
#include <chrono>
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
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(_window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
}

AppWindow::~AppWindow() {
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
    const auto frame_start = clock::now();
    glfwPollEvents();
    if (glfwGetWindowAttrib(_window, GLFW_ICONIFIED)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    draw_frame();
    ImGui::Render();

    int w, h;
    glfwGetFramebufferSize(_window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
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

void AppWindow::draw_frame() {
  // Network -> store, once per frame, on the UI thread (single owner).
  if (_receiver) {
    auto msgs = _receiver->drain();
    for (const auto &m : msgs)
      _store.ingest(m);
    const float dt = std::max(ImGui::GetIO().DeltaTime, 1e-3f);
    _msg_rate += 0.05 * (msgs.size() / dt - _msg_rate);
  }
  if (!_timeline.paused)
    _timeline.now = _store.latest_time();
  _timeline.jumped = false;
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
    if (_receiver && _receiver->dropped() > 0)
      ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "dropped %zu (overload)",
                         _receiver->dropped());
    ImGui::Separator();
    ImGui::TextDisabled("%.0f fps | MADS %s", ImGui::GetIO().Framerate,
                        app::mads_version());
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
      if (!ImGui::TreeNode(path.c_str(), "%s", comps[i].c_str())) {
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
  if (_store.series().empty())
    ImGui::TextDisabled("(no data yet)");
  ImGui::End();
}

void AppWindow::draw_source_menu(const std::string &key,
                                 const data::Series &s) {
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
  ImGui::SameLine();
  ImGui::SetNextItemWidth(200);
  float w = (float)_timeline.window;
  if (ImGui::SliderFloat("window [s]", &w, 1, 600, "%.0f",
                         ImGuiSliderFlags_Logarithmic))
    _timeline.window = w;
  ImGui::SameLine();
  const int t = (int)_timeline.now;
  ImGui::Text("t = %02d:%02d:%02d.%03d UTC  %s", t / 3600, t / 60 % 60, t % 60,
              (int)((_timeline.now - t) * 1000),
              _timeline.paused ? "(paused: pan/zoom freely)" : "(live)");
  ImGui::Text("%zu messages, %zu keypaths", _store.messages(),
              _store.series().size());
  // Review: while paused, scrub `now` over everything received.
  ImGui::BeginDisabled(!_timeline.paused || _store.messages() == 0);
  ImGui::SetNextItemWidth(-FLT_MIN);
  double now = _timeline.now;
  const double t0 = _store.first_time(), t1 = _store.latest_time();
  if (ImGui::SliderScalar("##scrub", ImGuiDataType_Double, &now, &t0, &t1,
                          "%.3f s")) {
    _timeline.now = now;
    _timeline.jumped = true;
  }
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
