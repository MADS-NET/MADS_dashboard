#include "ui/im_assert.hpp"

#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>

bool dash_im_assert_throws = false;

void dash_im_assert(const char *expr, const char *file, int line) {
  const std::string where = std::string(file) + ":" + std::to_string(line);
  if (dash_im_assert_throws)
    throw std::logic_error("IM_ASSERT(" + std::string(expr) + ") at " + where);
  static std::set<std::string> seen; // log each failing location once
  if (seen.insert(where).second)
    std::fprintf(stderr, "ImGui assertion failed: %s at %s\n", expr,
                 where.c_str());
}
