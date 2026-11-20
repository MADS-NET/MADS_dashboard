#pragma once

#include "ui/view.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace mads_dash::ui {

/// A restored dashboard layout.
struct Layout {
  std::vector<std::unique_ptr<View>> views;
  double window = 10.0;  ///< Timeline::window
  std::string imgui_ini; ///< ImGui::SaveIniSettingsToMemory() text, opaque
  int next_view_id = 1;
};

nlohmann::json layout_to_json(const std::vector<std::unique_ptr<View>> &views,
                              double window, const std::string &imgui_ini);

/// Unknown view types and malformed entries are skipped with a message in
/// `warnings`; never throws on bad content.
Layout layout_from_json(const nlohmann::json &j,
                        std::vector<std::string> &warnings);

/// Throws std::runtime_error on I/O failure.
void save_layout_file(const std::filesystem::path &, const nlohmann::json &);
/// Throws std::runtime_error (missing file, bad JSON).
nlohmann::json load_layout_file(const std::filesystem::path &);

/// Per-user config dir ($XDG_CONFIG_HOME or ~/.config on Linux,
/// ~/Library/Application Support on macOS, %APPDATA% on Windows)
/// + "/mads-dashboard/session.json".
std::filesystem::path session_file();

} // namespace mads_dash::ui
