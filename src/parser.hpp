#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace ash
{

    enum class RedirectionMode
    {
        TRUNCATE,
        APPEND
    };

    struct RedirectionSpec
    {
        std::string stdout_filename;
        std::string stderr_filename;
        bool has_stdout_redirection = false;
        bool has_stderr_redirection = false;
        RedirectionMode stdout_mode = RedirectionMode::TRUNCATE;
        RedirectionMode stderr_mode = RedirectionMode::TRUNCATE;
    };

    struct CommandSpec
    {
        std::string command;
        std::string args;
        RedirectionSpec redirection;
    };

    auto parse_command_and_position(const std::string& input) -> std::pair<std::string, size_t>;
    auto parse_arguments(const std::string& args) -> std::vector<std::string>;
    auto parse_and_strip_redirection(std::string& args) -> RedirectionSpec;
    auto parse_pipeline(const std::string& input) -> std::vector<CommandSpec>;
    auto parse_command_segment(const std::string& segment) -> std::optional<CommandSpec>;
    auto has_pipes(const std::string& input) -> bool;
    auto extract_filename_from_arguments(const std::string& args, size_t offset)
        -> std::optional<std::string>;
    auto trim_whitespace(const std::string& str) -> std::string;

} // namespace ash
