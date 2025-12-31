#pragma once

#include "parser.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ash {
// Lifecycle
auto initialize_shell() -> void;
auto cleanup_shell() -> void;

// REPL
auto read_input(const char *prompt) -> std::optional<std::string>;
auto command_completion(const char *text, int start, int end) -> char **;
auto command_generator(const char *text, int state) -> char *;
auto repl_loop() -> void;

// Input handling
auto handle_input(const std::string &input) -> bool;
auto handle_invalid_command(const std::string &input) -> void;

// Execution
auto execute_builtin(const std::string &command, const std::string &args)
    -> void;
auto execute_command(const std::string &command, const std::string &args,
                     RedirectionSpec redirection_spec) -> bool;
auto execute_pipeline(const std::vector<CommandSpec> &commands) -> bool;

} // namespace ash
