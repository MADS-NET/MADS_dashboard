#pragma once

#include <string>

struct GLFWwindow;

namespace mads_dash::ui {

/// Owns the GLFW window, the ImGui/ImPlot/ImPlot3D contexts and the main
/// render loop. One instance per process, created and run on the main thread.
class AppWindow {
public:
  struct Options {
    std::string title = "MADS Dashboard";
    int width = 1600;
    int height = 1000;
    int target_fps = 60; ///< frame cap; 0 = vsync only
  };

  explicit AppWindow(Options opts);
  ~AppWindow();
  AppWindow(const AppWindow &) = delete;
  AppWindow &operator=(const AppWindow &) = delete;

  /// Runs until the window is closed. Returns the process exit code.
  int run();

private:
  void draw_frame();
  void build_default_layout(unsigned int dockspace_id);

  Options _opts;
  GLFWwindow *_window = nullptr;
  bool _layout_built = false;
};

} // namespace mads_dash::ui
