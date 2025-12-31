#include "shell.hpp"
#include "commands.hpp"

#include <iostream>

#ifdef _WIN32
// TODO(abi): ...

#else

#include <readline/history.h>
#include <readline/readline.h>
#include <sys/wait.h>
#include <unistd.h>

#endif

auto initialize_shell() -> void {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  auto histfile = get_histfile();
  if (histfile.has_value()) {
    load_history_from_file(histfile.value());
  }

  rl_attempted_completion_function = command_completion;
}

auto cleanup_shell() -> void {
  auto histfile = get_histfile();
  if (histfile.has_value()) {
    write_history_to_file(histfile.value(), true);
  }
}

auto read_input(const char *prompt) -> std::optional<std::string> {
  char *input_cstr = readline(prompt);
  if (input_cstr == nullptr) {
    return std::nullopt;
  }

  std::string input(input_cstr);
  free(input_cstr);

  return input;
}

auto command_completion(const char *text, int start, int end) -> char ** {
  if (start == 0) {
    return rl_completion_matches(text, command_generator);
  }

  rl_attempted_completion_over = 1;
  return nullptr;
}

auto command_generator(const char *text, int state) -> char * {
  static std::vector<std::string> all_matches;
  static size_t match_index;

  if (state == 0) {
    all_matches.clear();
    match_index = 0;

    size_t len = strlen(text);

    // Builtins
    static const char *builtins[] = {"echo", "exit", nullptr};
    for (int i = 0; builtins[i] != nullptr; i++) {
      if (strncmp(builtins[i], text, len) == 0) {
        all_matches.push_back(builtins[i]);
      }
    }

    // Executables
    std::vector<std::string> executables =
        get_matching_executables_in_path(text);
    all_matches.insert(all_matches.end(), executables.begin(),
                       executables.end());
  }

  if (match_index < all_matches.size()) {
    return strdup(all_matches[match_index++].c_str());
  }

  return nullptr;
}

auto repl_loop() -> void {
  while (true) {
    auto input = read_input("$ ");
    if (!input.has_value()) {
      break;
    }

    if (!handle_input(input.value())) {
      break;
    }
  }
}

auto handle_input(const std::string &input) -> bool {
  if (!input.empty()) {
    command_history.push_back(input);
    add_history(input.c_str());
  }

  if (has_pipes(input)) {
    auto commands = parse_pipeline(input);
    execute_pipeline(commands);

    return true;
  }

  auto [command, command_end_pos] = parse_command_and_position(input);
  if (command.empty()) {
    return true;
  }

  if (command == "exit") {
    return false;
  }

  std::string args = (command_end_pos < input.length())
                         ? input.substr(command_end_pos + 1)
                         : "";
  RedirectionSpec redirection_spec = parse_redirection(args);

  auto success = execute_command(command, args, redirection_spec);
  if (!success) {
    handle_invalid_command(command);
  }

  return true;
}

auto handle_invalid_command(const std::string &command) -> void {
  std::cout << command << ": command not found" << std::endl;
}

auto execute_builtin(const std::string &command, const std::string &args)
    -> void {
  if (command == "echo") {
    echo_command(parse_arguments(args));
  } else if (command == "type") {
    type_command(args);
  } else if (command == "pwd") {
    pwd_command();
  } else if (command == "cd") {
    cd_command(args);
  } else if (command == "history") {
    history_command(args);
  }
}

auto execute_command(const std::string &command, const std::string &args,
                     RedirectionSpec redirection_spec) -> bool {
  std::string executable_path;
  if (!is_builtin(command)) {
    executable_path = find_executable_in_path(command);
    if (executable_path.empty()) {
      return false;
    }
  }

  bool needs_fork = redirection_spec.has_stdout_redirection ||
                    redirection_spec.has_stderr_redirection ||
                    !is_builtin(command);
  if (needs_fork) {
    pid_t pid = fork();
    if (pid == -1) {
      std::cerr << "Failed to fork process" << std::endl;
      return false;
    }

    // Child process
    if (pid == 0) {
      if (redirection_spec.has_stdout_redirection) {
        if (!redirect_stream(STDOUT_FILENO, redirection_spec.stdout_filename,
                             redirection_spec.stdout_mode)) {
          exit(1);
        }
      }

      if (redirection_spec.has_stderr_redirection) {
        if (!redirect_stream(STDERR_FILENO, redirection_spec.stderr_filename,
                             redirection_spec.stderr_mode)) {
          exit(1);
        }
      }

      if (is_builtin(command)) {
        execute_builtin(command, args);
        exit(0);
      }

      // Program name
      std::string program_name;
      size_t last_slash = executable_path.rfind('/');
      if (last_slash != std::string::npos) {
        program_name = executable_path.substr(last_slash + 1);
      }

      // Arguments
      // NOTE(abi): the C-like version needs to be null-terminated.
      std::vector<std::string> parsed_args = parse_arguments(args);
      std::vector<char *> c_args;
      c_args.push_back(const_cast<char *>(program_name.c_str()));
      for (const auto &arg : parsed_args) {
        c_args.push_back(const_cast<char *>(arg.c_str()));
      }
      c_args.push_back(nullptr);

      execvp(executable_path.c_str(), c_args.data());

      std::cerr << executable_path << ": command not found" << std::endl;
      exit(1);
    }

    // Parent process
    int status;
    waitpid(pid, &status, 0);
    return true;
  }

  execute_builtin(command, args);
  return true;
}

auto execute_pipeline(const std::vector<CommandSpec> &commands) -> bool {
  if (commands.empty()) {
    return false;
  }

  // Single command, no pipeline
  if (commands.size() == 1) {
    return execute_command(commands[0].command, commands[0].args,
                           commands[0].redirection);
  }

  // Create pipes
  int num_commands = commands.size();
  int pipes[num_commands - 1][2];
  for (int i = 0; i < num_commands - 1; i++) {
    if (pipe(pipes[i]) == -1) {
      std::cerr << "Failed to create pipe" << std::endl;
      return false;
    }
  }

  // Fork and execute commands
  std::vector<pid_t> pids;
  for (int i = 0; i < num_commands; i++) {
    const CommandSpec &cmd = commands[i];

    std::string executable_path;
    if (!is_builtin(cmd.command)) {
      executable_path = find_executable_in_path(cmd.command);
      if (executable_path.empty()) {
        for (int j = 0; j < num_commands - 1; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }
        std::cerr << cmd.command << ": command not found" << std::endl;
        return false;
      }
    }

    pid_t pid = fork();
    if (pid == -1) {
      std::cerr << "Failed to fork process" << std::endl;
      return false;
    }

    if (pid == 0) {
      // Redirect stdin from the previous pipe, if it's not the first command
      if (i > 0) {
        dup2(pipes[i - 1][0], STDIN_FILENO);
      }

      // Redirect stdout to the next pipe, if it's not the last command
      if (i < num_commands - 1) {
        dup2(pipes[i][1], STDOUT_FILENO);
      }

      // Close all pipe file descriptors in child
      for (int j = 0; j < num_commands - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }

      // Handle file redirections
      if (cmd.redirection.has_stdout_redirection) {
        if (!redirect_stream(STDOUT_FILENO, cmd.redirection.stdout_filename,
                             cmd.redirection.stdout_mode)) {
          exit(1);
        }
      }

      if (cmd.redirection.has_stderr_redirection) {
        if (!redirect_stream(STDERR_FILENO, cmd.redirection.stderr_filename,
                             cmd.redirection.stderr_mode)) {
          exit(1);
        }
      }

      // Builtin
      if (is_builtin(cmd.command)) {
        execute_builtin(cmd.command, cmd.args);
        exit(0);
      }

      // External command
      std::string program_name;
      size_t last_slash = executable_path.rfind('/');
      if (last_slash != std::string::npos) {
        program_name = executable_path.substr(last_slash + 1);
      } else {
        program_name = executable_path;
      }

      std::vector<std::string> parsed_args = parse_arguments(cmd.args);
      std::vector<char *> c_args;
      c_args.push_back(const_cast<char *>(program_name.c_str()));
      for (const auto &arg : parsed_args) {
        c_args.push_back(const_cast<char *>(arg.c_str()));
      }
      c_args.push_back(nullptr);

      execvp(executable_path.c_str(), c_args.data());

      std::cerr << executable_path << ": command not found" << std::endl;
      exit(1);
    }

    pids.push_back(pid);
  }

  // NOTE(abi): we must close all pipes so that commands reading from stdin get
  // EOF.
  for (int i = 0; i < num_commands - 1; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

  // Wait for all children to complete
  for (pid_t pid : pids) {
    int status;
    waitpid(pid, &status, 0);
  }

  return true;
}
