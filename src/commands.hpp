#pragma once

#include "parser.hpp"

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

extern const std::unordered_set<std::string> SHELL_BUILTINS;
extern std::string previous_directory;
extern std::vector<std::string> command_history;
extern size_t command_history_last_write_index;

// Builtin commands
auto echo_command(const std::vector<std::string> &args) -> void;
auto type_command(const std::string &name) -> void;
auto pwd_command() -> void;
auto cd_command(const std::string &path) -> void;
auto history_command(const std::string &args) -> void;
auto is_builtin(const std::string &command) -> bool;

// History
auto get_histfile() -> std::optional<std::string>;
auto load_history_from_file(const std::string &filepath) -> bool;
auto write_history_to_file(const std::string &filepath, bool append = false)
    -> bool;

// Path utilities (for external executables)
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
