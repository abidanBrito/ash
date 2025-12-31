#include "parser.hpp"

namespace ash {

auto parse_command_and_position(const std::string &input)
    -> std::pair<std::string, size_t> {
  if (input.empty()) {
    return {"", 0};
  }

  std::string command;
  bool in_single_quotes = false;
  bool in_double_quotes = false;

  size_t i = 0;
  for (; i < input.size(); i++) {
    char c = input[i];

    if (!in_double_quotes && c == '\'') {
      in_single_quotes = !in_single_quotes;
    } else if (!in_single_quotes && c == '\"') {
      in_double_quotes = !in_double_quotes;
    } else if (!in_single_quotes && !in_double_quotes &&
               (c == ' ' || c == '\t')) {
      break;
    } else {
      command += c;
    }
  }

  return {command, i};
}

auto parse_arguments(const std::string &args) -> std::vector<std::string> {
  std::vector<std::string> parsed_args;
  std::string current_arg;
  bool in_single_quotes = false;
  bool in_double_quotes = false;

  for (size_t i = 0; i < args.length(); i++) {
    char c = args[i];

    // Double quotes
    if (c == '\"' && !in_single_quotes) {
      in_double_quotes = !in_double_quotes;
    }

    // Single quotes
    else if (c == '\'' && !in_double_quotes) {
      in_single_quotes = !in_single_quotes;
    }

    // Whitespace
    else if (c == ' ' && !in_double_quotes && !in_single_quotes) {
      if (!current_arg.empty()) {
        parsed_args.push_back(current_arg);
        current_arg.clear();
      }
    }

    // Escaped characters
    else if (c == '\\' && !in_single_quotes && i + 1 < args.length()) {
      if (!in_double_quotes || args[i + 1] == '\"' || args[i + 1] == '\\') {
        current_arg += args[++i];
      } else {
        current_arg += '\\';
      }
    }

    else {
      current_arg += c;
    }
  }

  if (!current_arg.empty()) {
    parsed_args.push_back(current_arg);
  }

  return parsed_args;
}

auto parse_and_strip_redirection(std::string &args) -> RedirectionSpec {
  RedirectionSpec redirection_spec;

  size_t stderr_pos = std::string::npos;
  size_t stderr_len = 0;
  size_t stdout_pos = std::string::npos;
  size_t stdout_len = 0;

  // Stderr append (2>>)
  stderr_len = 3;
  stderr_pos = args.find("2>>");
  if (stderr_pos != std::string::npos) {
    size_t filename_start = stderr_pos + stderr_len;
    while (filename_start < args.length() &&
           (args[filename_start] == ' ' || args[filename_start] == '\t')) {
      filename_start++;
    }

    if (filename_start < args.length()) {
      size_t filename_end = filename_start;
      while (filename_end < args.length() && args[filename_end] != ' ' &&
             args[filename_end] != '\t') {
        filename_end++;
      }

      redirection_spec.stderr_filename =
          args.substr(filename_start, filename_end - filename_start);
      redirection_spec.has_stderr_redirection = true;
      redirection_spec.stderr_mode = RedirectionMode::APPEND;
    }
  }

  // Stderr overwrite (2>)
  if (redirection_spec.stderr_mode != RedirectionMode::APPEND) {
    stderr_len = 2;
    stderr_pos = args.find("2>");
    if (stderr_pos != std::string::npos) {
      size_t filename_start = stderr_pos + stderr_len;
      while (filename_start < args.length() &&
             (args[filename_start] == ' ' || args[filename_start] == '\t')) {
        filename_start++;
      }

      if (filename_start < args.length()) {
        size_t filename_end = filename_start;
        while (filename_end < args.length() && args[filename_end] != ' ' &&
               args[filename_end] != '\t') {
          filename_end++;
        }

        redirection_spec.stderr_filename =
            args.substr(filename_start, filename_end - filename_start);
        redirection_spec.has_stderr_redirection = true;
      }
    }
  }

  // Stdout append (1>> or >>)
  stdout_len = 0;
  stdout_pos = args.find("1>>");
  if (stdout_pos != std::string::npos) {
    stdout_len = 3;
  } else {
    size_t pos = 0;
    while ((pos = args.find(">>", pos)) != std::string::npos) {
      if (pos > 0 && args[pos - 1] == '2') {
        pos += 2;
        continue;
      }
      stdout_pos = pos;
      stdout_len = 2;
      break;
    }
  }

  if (stdout_pos != std::string::npos) {
    size_t filename_start = stdout_pos + stdout_len;
    if (filename_start == args.length()) {
      return redirection_spec;
    }

    while (filename_start < args.length() && args[filename_start] == ' ') {
      filename_start++;
    }

    size_t filename_end = filename_start;
    while (filename_end < args.length() && args[filename_end] != ' ') {
      filename_end++;
    }

    redirection_spec.stdout_filename =
        args.substr(filename_start, filename_end - filename_start);
    redirection_spec.has_stdout_redirection = true;
    redirection_spec.stdout_mode = RedirectionMode::APPEND;
  }

  // Stdout overwrite (> or 1>)
  if (redirection_spec.stdout_mode != RedirectionMode::APPEND) {
    stdout_len = 0;
    stdout_pos = args.find("1>");
    if (stdout_pos != std::string::npos) {
      stdout_len = 2;
    } else {
      size_t pos = 0;
      while ((pos = args.find(">", pos)) != std::string::npos) {
        if (pos > 0 && args[pos - 1] == '2') {
          pos += 2;
          continue;
        }

        if (pos + 1 < args.length() && args[pos + 1] == '>') {
          pos += 2;
          continue;
        }

        stdout_pos = pos;
        stdout_len = 1;
        break;
      }
    }

    if (stdout_pos != std::string::npos) {
      size_t filename_start = stdout_pos + stdout_len;
      if (filename_start == args.length()) {
        return redirection_spec;
      }

      while (filename_start < args.length() && args[filename_start] == ' ') {
        filename_start++;
      }

      size_t filename_end = filename_start;
      while (filename_end < args.length() && args[filename_end] != ' ') {
        filename_end++;
      }

      redirection_spec.stdout_filename =
          args.substr(filename_start, filename_end - filename_start);
      redirection_spec.has_stdout_redirection = true;
    }
  }

  // Clean arguments string
  size_t first_redirection_pos = std::string::npos;
  if (stdout_pos != std::string::npos && stderr_pos != std::string::npos) {
    first_redirection_pos = std::min(stdout_pos, stderr_pos);
  } else if (stdout_pos != std::string::npos) {
    first_redirection_pos = stdout_pos;
  } else if (stderr_pos != std::string::npos) {
    first_redirection_pos = stderr_pos;
  }

  if (first_redirection_pos != std::string::npos) {
    args = args.substr(0, first_redirection_pos);
  }

  return redirection_spec;
}

auto parse_pipeline(const std::string &input) -> std::vector<CommandSpec> {
  std::vector<CommandSpec> commands;

  std::string current_segment;
  bool in_single_quotes = false;
  bool in_double_quotes = false;

  for (char c : input) {
    if (!in_double_quotes && c == '\'') {
      in_single_quotes = !in_single_quotes;
      current_segment += c;
    } else if (!in_single_quotes && c == '\"') {
      in_double_quotes = !in_double_quotes;
      current_segment += c;
    } else if (!in_single_quotes && !in_double_quotes && c == '|') {
      if (auto cmd = parse_command_segment(current_segment)) {
        commands.push_back(*cmd);
      }
      current_segment.clear();
    } else {
      current_segment += c;
    }
  }

  if (auto cmd = parse_command_segment(current_segment)) {
    commands.push_back(*cmd);
  }

  return commands;
}

auto parse_command_segment(const std::string &segment)
    -> std::optional<CommandSpec> {
  std::string trimmed = trim_whitespace(segment);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  auto [command, command_end_pos] = parse_command_and_position(trimmed);
  std::string args = (command_end_pos < trimmed.length())
                         ? trimmed.substr(command_end_pos + 1)
                         : "";
  RedirectionSpec redirection_spec = parse_and_strip_redirection(args);

  return CommandSpec{command, args, redirection_spec};
}

auto has_pipes(const std::string &input) -> bool {
  bool in_single_quotes = false;
  bool in_double_quotes = false;

  for (char c : input) {
    if (!in_double_quotes && c == '\'') {
      in_single_quotes = !in_single_quotes;
    } else if (!in_single_quotes && c == '\"') {
      in_double_quotes = !in_double_quotes;
    } else if (!in_single_quotes && !in_double_quotes && c == '|') {
      return true;
    }
  }

  return false;
}

auto extract_filename_from_arguments(const std::string &args, size_t offset)
    -> std::optional<std::string> {
  size_t filename_start = args.find_first_not_of(" \t", offset);
  if (filename_start == std::string::npos) {
    return std::nullopt;
  }

  std::string filename = args.substr(filename_start);

  size_t filename_end = filename.find_last_not_of(" \t");
  if (filename_end != std::string::npos) {
    filename = filename.substr(0, filename_end + 1);
  }

  return filename;
}

auto trim_whitespace(const std::string &str) -> std::string {
  size_t start = str.find_first_not_of(" \t");
  if (start == std::string::npos) {
    return "";
  }

  size_t end = str.find_last_not_of(" \t");
  return str.substr(start, end - start + 1);
}

} // namespace ash
