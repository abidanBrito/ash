#include <cstdlib>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <algorithm>
#include <dirent.h>

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

const std::unordered_set<std::string> SHELL_BUILTINS = {
    "exit", "echo", "type", "pwd", "cd", "history"};

std::string previous_directory;
std::vector<std::string> command_history;

enum class RedirectionMode { TRUNCATE, APPEND };

struct RedirectionSpec {
  std::string stdout_filename;
  std::string stderr_filename;
  bool has_stdout_redirection = false;
  bool has_stderr_redirection = false;
  RedirectionMode stdout_mode = RedirectionMode::TRUNCATE;
  RedirectionMode stderr_mode = RedirectionMode::TRUNCATE;
};

struct CommandSpec {
  std::string command;
  std::string args;
  RedirectionSpec redirection;
};

// REPL
auto read_input(const char *prompt) -> std::optional<std::string>;
auto command_completion(const char *text, int start, int end) -> char **;
auto command_generator(const char *text, int state) -> char *;
auto repl_loop() -> void;

// Parsing
auto parse_command_and_position(const std::string &input)
    -> std::pair<std::string, size_t>;
auto parse_arguments(const std::string &args) -> std::vector<std::string>;
auto parse_redirection(std::string &args) -> RedirectionSpec;
auto parse_pipeline(const std::string &input) -> std::vector<CommandSpec>;
auto has_pipes(const std::string &input) -> bool;

// Input handling
auto handle_input(const std::string &input) -> bool;
auto handle_invalid_command(const std::string &input) -> void;

// Builtin commands
auto echo_command(const std::vector<std::string> &args) -> void;
auto type_command(const std::string &name) -> void;
auto pwd_command() -> void;
auto cd_command(const std::string &path) -> void;
auto history_command(const std::string &args) -> void;
auto is_builtin(const std::string &command) -> bool;

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

auto main() -> int {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  rl_attempted_completion_function = command_completion;
  repl_loop();

  return 0;
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

auto parse_redirection(std::string &args) -> RedirectionSpec {
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
      if (!current_segment.empty()) {
        // Trim whitespace
        size_t start = current_segment.find_first_not_of(" \t");
        size_t end = current_segment.find_last_not_of(" \t");
        if (start != std::string::npos) {
          std::string trimmed = current_segment.substr(start, end - start + 1);

          // Parse segment
          auto [command, command_end_pos] = parse_command_and_position(trimmed);
          std::string args = (command_end_pos < trimmed.length())
                                 ? trimmed.substr(command_end_pos + 1)
                                 : "";
          RedirectionSpec redirection_spec = parse_redirection(args);

          commands.push_back({command, args, redirection_spec});
        }
      }
      current_segment.clear();
    } else {
      current_segment += c;
    }
  }

  // Process last segment
  if (!current_segment.empty()) {
    size_t start = current_segment.find_first_not_of(" \t");
    size_t end = current_segment.find_last_not_of(" \t");
    if (start != std::string::npos) {
      std::string trimmed = current_segment.substr(start, end - start + 1);

      auto [command, command_end_pos] = parse_command_and_position(trimmed);
      std::string args = (command_end_pos < trimmed.length())
                             ? trimmed.substr(command_end_pos + 1)
                             : "";
      RedirectionSpec redirection_spec = parse_redirection(args);

      commands.push_back({command, args, redirection_spec});
    }
  }

  return commands;
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

auto echo_command(const std::vector<std::string> &args) -> void {
  for (size_t i = 0; i < args.size(); i++) {
    std::cout << args[i];
    if (i < args.size() - 1) {
      std::cout << " ";
    }
  }

  std::cout << std::endl;
}

auto type_command(const std::string &name) -> void {
  if (is_builtin(name)) {
    std::cout << name << " is a shell builtin" << std::endl;
    return;
  }

  if (auto filepath = find_executable_in_path(name); !filepath.empty()) {
    std::cout << name << " is " << filepath << std::endl;
    return;
  }

  std::cout << name << ": not found" << std::endl;
}

auto pwd_command() -> void {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    std::cout << cwd << std::endl;
    return;
  }

  std::cerr << "pwd: error getting the current working directory" << std::endl;
}

auto cd_command(const std::string &path) -> void {
  std::string target_path;

  // Home directory
  if (path.empty() || path == "~") {
    const char *home = std::getenv("HOME");
    if (home == nullptr) {
      std::cerr << "cd: HOME not set" << std::endl;
      return;
    }
    target_path = home;
  }
  // Previous directory
  else if (path == "-") {
    if (previous_directory.empty()) {
      pwd_command();
      return;
    }

    target_path = previous_directory;
  }
  // Absolute/relative path
  else {
    target_path = path;
  }

  char current_dir[1024];
  if (getcwd(current_dir, sizeof(current_dir)) != nullptr) {
    if (chdir(target_path.c_str()) != 0) {
      std::cout << "cd: " << path << ": No such file or directory" << std::endl;
    } else {
      previous_directory = current_dir;
    }
  }
}

auto history_command(const std::string &args) -> void {
  if (args.find("-r") == 0) {
    // Filename
    size_t filename_start = args.find_first_not_of(" \t", 2);
    if (filename_start == std::string::npos) {
      std::cerr << "history: -r requires a filename" << std::endl;
      return;
    }

    std::string filename = args.substr(filename_start);

    size_t filename_end = filename.find_last_not_of(" \t");
    if (filename_end != std::string::npos) {
      filename = filename.substr(0, filename_end + 1);
    }

    // Read history file
    std::ifstream file(filename);
    if (!file.is_open()) {
      std::cerr << "history: cannot open " << filename << std::endl;
      return;
    }

    // Parse commands & add them both history stores
    std::string line;
    while (std::getline(file, line)) {
      if (line.empty()) {
        continue;
      }

      command_history.push_back(line);
      add_history(line.c_str());
    }

    file.close();
    return;
  }

  int num_entries = command_history.size();

  if (!args.empty()) {
    try {
      num_entries = std::stoi(args);
    } catch (...) {
      std::cerr << "history: invalid argument" << std::endl;
      return;
    }
  }

  size_t start_index = 0;
  if (num_entries < static_cast<int>(command_history.size())) {
    start_index = command_history.size() - num_entries;
  }

  for (size_t i = start_index; i < command_history.size(); i++) {
    std::cout << std::setw(5) << (i + 1) << "  " << command_history[i]
              << std::endl;
  }
}

auto is_builtin(const std::string &command) -> bool {
  return SHELL_BUILTINS.find(command) != SHELL_BUILTINS.end();
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

auto split_path(const std::string &path) -> std::vector<std::string> {
  std::vector<std::string> directories;
  std::stringstream ss(path);
  std::string directory;

  while (std::getline(ss, directory, PATH_LIST_SEPARATOR)) {
    directories.push_back(directory);
  }

  return directories;
}

auto get_path_directories() -> std::vector<std::string> {
  const char *path_cstr = std::getenv("PATH");
  if (path_cstr == nullptr) {
    return {};
  }

  return split_path(path_cstr);
}

auto find_executable_in_path(const std::string &command) -> std::string {
  std::vector<std::string> directories = get_path_directories();
  for (const std::string &dir : directories) {
    std::string filepath = dir + "/" + command;
    if (is_executable(filepath)) {
      return filepath;
    }
  }

  return "";
}

auto get_matching_executables_in_path(const std::string &prefix, bool sort)
    -> std::vector<std::string> {
  std::unordered_set<std::string> unique_executables;

  std::vector<std::string> directories = get_path_directories();
  for (const std::string &dir : directories) {
    // Check if the directory exists
    struct stat st;
    if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
      continue;
    }

    // Open the directory
    DIR *dirp = opendir(dir.c_str());
    if (dirp == nullptr) {
      continue;
    }

    // Read entries
    struct dirent *entry;
    while ((entry = readdir(dirp)) != nullptr) {
      std::string name = entry->d_name;
      if (name == "." || name == "..") {
        continue;
      }

      if (name.find(prefix) == 0) {
        if (is_executable(dir + "/" + name)) {
          unique_executables.insert(name);
        }
      }
    }

    closedir(dirp);
  }

  std::vector<std::string> matches(unique_executables.begin(),
                                   unique_executables.end());
  if (sort) {
    std::sort(matches.begin(), matches.end());
  }

  return matches;
}

#ifdef _WIN32
auto is_executable(const std::string &filepath) -> bool {
  DWORD attributes = GetFileAttributesA(filepath.c_str());

  if (attributes == INVALID_FILE_ATTRIBUTES ||
      (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    return false;
  }

  size_t dot_pos = filepath.rfind('.');
  if (dot_pos != std::string::npos) {
    std::string ext = filepath.substr(dot_pos);
    for (char &c : ext)
      c = std::tolower(c);

    if (ext == ".exe" || ext == ".bat" || ext == ".ps1" || ext == ".cmd" ||
        ext == ".com") {
      return true;
    }
  }

  return false;
}

#else
auto is_executable(const std::string &filepath) -> bool {
  struct stat file_stat;
  if (stat(filepath.c_str(), &file_stat) != 0) {
    return false;
  }

  if (!S_ISREG(file_stat.st_mode)) {
    return false;
  }

  return access(filepath.c_str(), X_OK) == 0;
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
#endif

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

auto get_redirection_file_descriptor_flags(RedirectionMode mode) -> int {
  int flags = O_WRONLY | O_CREAT;
  flags |= (mode == RedirectionMode::APPEND) ? O_APPEND : O_TRUNC;

  return flags;
}

auto redirect_stream(int stream_file_descriptor, const std::string &filename,
                     RedirectionMode mode) -> bool {
  int fd =
      open(filename.c_str(), get_redirection_file_descriptor_flags(mode), 0644);
  if (fd == -1) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return false;
  }

  int duplicate_fd = dup2(fd, stream_file_descriptor);
  if (duplicate_fd == -1) {
    std::cerr << "Failed to redirect output" << std::endl;
    close(fd);
    return false;
  }

  close(fd);
  return true;
}
