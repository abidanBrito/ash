#include "shell.hpp"

auto main() -> int
{
    ash::initialize_shell();
    ash::repl_loop();
    ash::cleanup_shell();

    return 0;
}
