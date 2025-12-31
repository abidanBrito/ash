#include "shell.hpp"

auto main() -> int {
  initialize_shell();
  repl_loop();
  cleanup_shell();

  return 0;
}
