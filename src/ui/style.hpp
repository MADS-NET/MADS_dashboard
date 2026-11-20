#pragma once

#include <imgui.h>

namespace mads_dash::ui {

/// Same look as the MADS GUIs (mads-chat, mads-director: MADS
/// src/gui/common/gui_style.cpp), which MADS does not install.
void apply_style();
/// First available system monospace font (Menlo, DejaVu Sans Mono, Consolas,
/// ...), else ImGui's default font.
ImFont *load_monospace_font(float size_pixels = 15.0f);
inline constexpr ImVec4 kClearColor{0.06f, 0.06f, 0.08f, 1.0f};

} // namespace mads_dash::ui
