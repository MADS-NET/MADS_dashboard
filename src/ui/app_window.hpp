#pragma once

#include "data/store.hpp"
#include "net/receiver.hpp"
#include "ui/view.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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
    std::string layout; ///< layout file to open instead of the last session
    bool stats = false; ///< print performance statistics every second
    std::chrono::steady_clock::time_point started; ///< process start
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
  void draw_node_menu(const std::string &path);
  void draw_timeline();
  void draw_views();
  void build_default_layout(unsigned int dockspace_id);
  void connect(std::vector<std::string> args);
  void apply_layout(const nlohmann::json &j);
  nlohmann::json current_layout() const;
  void file_actions(); // Save/Load layout, Export HDF5 menu items
  void notify(std::string text, bool error = false);

  Options _opts;
  GLFWwindow *_window = nullptr;
  bool _layout_built = false;

  std::unique_ptr<net::Receiver> _receiver;
  data::Store _store;
  Timeline _timeline;
  std::vector<std::unique_ptr<View>> _views;
  int _next_view_id = 1;
  unsigned int _views_dock = 0;
  unsigned int _logo_tex = 0; ///< GL texture, Views panel background
  double _msg_rate = 0; ///< smoothed messages per second
  double _buffer = 600; ///< store retention in seconds, 0 = unlimited
  double _last_trim = 0;
  /// (topic, time field) chosen in a tree menu, applied after the tree is
  /// drawn: set_time_field() erases keypaths the tree is iterating over.
  std::optional<std::pair<std::string, std::string>> _pending_time;

  // --stats: counters of the current one-second period
  void stats_message(const data::Message &m, double now_utc);
  void stats_tick();
  bool _first_frame = true;
  double _stat_t0 = 0;
  int _stat_frames = 0;
  std::size_t _stat_msgs = 0;
  std::vector<double> _stat_lags; ///< ms, message timestamp -> frame

  std::string _pending_ini; ///< ImGui ini to load before the next frame
  std::string _notice;      ///< last file action result, shown in the bar
  bool _notice_error = false;

  bool _open_connect = false;
  int _connect_mode = 0; // 0 = settings URI, 1 = room
  char _uri[256] = "tcp://localhost:9092";
  char _room[64] = "mads";
  char _topics[256] = "";
  bool _crypto = false;
  char _keys_dir[512] = "";
  char _client_key[64] = "client";
  char _broker_key[64] = "broker";
};

} // namespace mads_dash::ui
