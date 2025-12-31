#pragma once

#include <string>
#include <vector>

namespace ash {

struct ShellState {
  std::string previous_directory;
  std::vector<std::string> command_history;
  size_t command_history_last_write_index = 0;
};

extern ShellState shell_state;
} // namespace ash
