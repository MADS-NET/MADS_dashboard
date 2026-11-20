#include "ui/app_window.hpp"

#include "app/version.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h> // DockBuilder API
#include <implot.h>
#include <implot3d.h>

#include <chrono>
#include <stdexcept>
#include <thread>

namespace mads_dash::ui {

namespace {
constexpr const char *kSourcesWin = "Sources";
constexpr const char *kViewsWin = "Views";
constexpr const char *kTimelineWin = "Timeline";
constexpr const char *kInspectorWin = "Inspector";

void glfw_error(int code, const char *msg) {
  fprintf(stderr, "GLFW error %d: %s\n", code, msg);
}
} // namespace

AppWindow::AppWindow(Options opts) : _opts(std::move(opts)) {
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

void AppWindow::draw_frame() {
  // Toolbar / menu
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Quit"))
        glfwSetWindowShouldClose(_window, GLFW_TRUE);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      ImGui::MenuItem("ImGui/ImPlot demo", nullptr, &_show_demo);
      ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::TextDisabled("MADS Dashboard %s | MADS %s | %.0f fps",
                        app::kVersion, app::mads_version(),
                        ImGui::GetIO().Framerate);
    ImGui::EndMainMenuBar();
  }

  const ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();
  if (!_layout_built) {
    build_default_layout(dockspace_id);
    _layout_built = true;
  }

  ImGui::Begin(kSourcesWin);
  ImGui::TextDisabled("(source tree: M1)");
  ImGui::End();

  ImGui::Begin(kViewsWin);
  ImGui::TextDisabled("(views: M1)");
  ImGui::End();

  ImGui::Begin(kInspectorWin);
  ImGui::TextDisabled("(selection properties: M1)");
  ImGui::End();

  ImGui::Begin(kTimelineWin);
  ImGui::TextDisabled("(timeline: M1)");
  ImGui::End();

  if (_show_demo) {
    ImGui::ShowDemoWindow(&_show_demo);
    ImPlot::ShowDemoWindow();
    ImPlot3D::ShowDemoWindow();
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
  ImGuiID bottom =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.18f, nullptr, &center);
  ImGuiID left =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);
  ImGuiID right =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, nullptr, &center);

  ImGui::DockBuilderDockWindow(kSourcesWin, left);
  ImGui::DockBuilderDockWindow(kInspectorWin, right);
  ImGui::DockBuilderDockWindow(kTimelineWin, bottom);
  ImGui::DockBuilderDockWindow(kViewsWin, center);
  ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace mads_dash::ui
