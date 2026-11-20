#include "ui/style.hpp"

#include <array>
#include <filesystem>

namespace mads_dash::ui {

void apply_style() {
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.ChildRounding = 6.0f;
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.0f);
  style.Colors[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.15f, 0.18f, 1.0f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.19f, 0.33f, 0.42f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.44f, 0.56f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.29f, 0.37f, 1.0f);
}

ImFont *load_monospace_font(float size_pixels) {
  ImGuiIO &io = ImGui::GetIO();
  const std::array<const char *, 6> candidates = {
      "/System/Library/Fonts/Menlo.ttc",
      "/System/Library/Fonts/SFNSMono.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
      "C:/Windows/Fonts/consola.ttf",
      "C:/Windows/Fonts/cour.ttf",
      "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
  };
  for (const char *candidate : candidates)
    if (std::filesystem::exists(candidate))
      if (ImFont *font = io.Fonts->AddFontFromFileTTF(candidate, size_pixels))
        return font;
  return io.Fonts->AddFontDefault();
}

} // namespace mads_dash::ui
