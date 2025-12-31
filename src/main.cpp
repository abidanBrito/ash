#include "shell.hpp"

#include <iostream>

auto main() -> int {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  auto histfile = get_histfile();
  if (histfile.has_value()) {
    load_history_from_file(histfile.value());
  }

  rl_attempted_completion_function = command_completion;

  repl_loop();

  if (histfile.has_value()) {
    write_history_to_file(histfile.value(), true);
  }

  return 0;
}
