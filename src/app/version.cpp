#include "app/version.hpp"

#include <agent_c.h>

namespace mads_dash::app {
const char *mads_version() { return ::mads_version(); }
} // namespace mads_dash::app
