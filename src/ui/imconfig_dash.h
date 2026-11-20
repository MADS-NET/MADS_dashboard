#pragma once
// Dear ImGui user config (IMGUI_USER_CONFIG). Keeps ImGui/ImPlot assertions
// active in Release: the app logs them, the tests make them throw.
void dash_im_assert(const char *expr, const char *file, int line);
#define IM_ASSERT(_EXPR)                                                       \
  ((_EXPR) ? (void)0 : dash_im_assert(#_EXPR, __FILE__, __LINE__))
