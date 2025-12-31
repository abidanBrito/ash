#pragma once

#include "parser.hpp"

#include <dirent.h>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32

#include <windows.h>
constexpr char PATH_LIST_SEPARATOR = ';';

#else

#include <fcntl.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

constexpr char PATH_LIST_SEPARATOR = ':';

#endif

// REPL
auto read_input(const char *prompt) -> std::optional<std::string>;
auto command_completion(const char *text, int start, int end) -> char **;
auto command_generator(const char *text, int state) -> char *;
auto repl_loop() -> void;

// Input handling
auto handle_input(const std::string &input) -> bool;
auto handle_invalid_command(const std::string &input) -> void;

// Builtin commands
auto echo_command(const std::vector<std::string> &args) -> void;
auto type_command(const std::string &name) -> void;
auto pwd_command() -> void;
auto cd_command(const std::string &path) -> void;
auto load_history_from_file(const std::string &filepath) -> bool;
auto write_history_to_file(const std::string &filepath, bool append = false)
    -> bool;
auto history_command(const std::string &args) -> void;
auto is_builtin(const std::string &command) -> bool;
auto get_histfile() -> std::optional<std::string>;

// External commands (executables)
auto split_path(const std::string &path) -> std::vector<std::string>;
auto get_path_directories() -> std::vector<std::string>;
auto find_executable_in_path(const std::string &command) -> std::string;
auto get_matching_executables_in_path(const std::string &prefix,
                                      bool sort = true)
    -> std::vector<std::string>;
auto is_executable(const std::string &filepath) -> bool;

// Redirection
auto redirect_stream(int stream_file_descriptor, const std::string &filename,
                     RedirectionMode mode) -> bool;
auto get_redirection_file_descriptor_flags(RedirectionMode mode) -> int;

// Execution
auto execute_builtin(const std::string &command, const std::string &args)
    -> void;
auto execute_command(const std::string &command, const std::string &args,
                     RedirectionSpec redirection_spec) -> bool;
auto execute_pipeline(const std::vector<CommandSpec> &commands) -> bool;
