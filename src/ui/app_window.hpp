#pragma once

#include "data/store.hpp"
#include "net/receiver.hpp"
#include "ui/view.hpp"

#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace mads_dash::ui {

/// Owns the GLFW window, the ImGui/ImPlot/ImPlot3D contexts, the receiver, the
/// store and the views; runs the render loop on the main thread.
class AppWindow {
public:
  struct Options {
    std::string title = "MADS Dashboard";
    int width = 1600;
    int height = 1000;
    int target_fps = 60;                    ///< frame cap; 0 = vsync only
    std::vector<std::string> receiver_args; ///< mads agent CLI options
  };

  explicit AppWindow(Options opts);
  ~AppWindow();
  AppWindow(const AppWindow &) = delete;
  AppWindow &operator=(const AppWindow &) = delete;

  /// Runs until the window is closed. Returns the process exit code.
  int run();

private:
  void draw_frame();
  void draw_menu_bar();
  void draw_connect_popup();
  void draw_sources();
  void draw_source_menu(const std::string &key, const data::Series &s);
  void draw_timeline();
  void draw_views();
  void build_default_layout(unsigned int dockspace_id);
  void connect(std::vector<std::string> args);

  Options _opts;
  GLFWwindow *_window = nullptr;
  bool _layout_built = false;

  std::unique_ptr<net::Receiver> _receiver;
  data::Store _store;
  Timeline _timeline;
  std::vector<std::unique_ptr<View>> _views;
  int _next_view_id = 1;
  unsigned int _views_dock = 0;
  double _msg_rate = 0; ///< smoothed messages per second

  bool _open_connect = false;
  int _connect_mode = 0; // 0 = settings URI, 1 = room
  char _uri[256] = "tcp://localhost:9092";
  char _room[64] = "mads";
  char _topics[256] = "";
};

} // namespace mads_dash::ui
