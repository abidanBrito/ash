#pragma once

#include <stddef.h>

namespace ash {

namespace config {
constexpr const char *PROMPT = "$ ";
constexpr size_t MAX_PATH_LENGTH = 1024;

} // namespace config

namespace permissions {
constexpr int DEFAULT_FILE_MODE = 0644;
} // namespace permissions

} // namespace ash
