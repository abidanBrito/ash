#include <cstdlib>

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef _WIN32

#include <windows.h>
constexpr char PATH_LIST_SEPARATOR = ';';

#else

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

constexpr char PATH_LIST_SEPARATOR = ':';

#endif

const std::unordered_set<std::string> SHELL_BUILTINS = {"exit", "echo", "type",
                                                        "pwd", "cd"};
std::string previous_directory;

struct RedirectionSpec {
  std::string stdout_filename;
  std::string stderr_filename;
  bool has_stdout_redirection = false;
  bool has_stderr_redirection = false;
};

auto print_prompt() -> void;
auto repl_loop() -> void;
auto parse_command_and_position(const std::string &input)
    -> std::pair<std::string, size_t>;
auto parse_arguments(const std::string &args) -> std::vector<std::string>;
auto parse_redirection(std::string &args) -> RedirectionSpec;
auto handle_input(const std::string &input) -> bool;
auto handle_invalid_command(const std::string &input) -> void;
auto echo_command(const std::vector<std::string> &args) -> void;
auto type_command(const std::string &name) -> void;
auto pwd_command() -> void;
auto cd_command(const std::string &path) -> void;
auto is_builtin(const std::string &command) -> bool;
auto execute_builtin(const std::string &command, const std::string &args)
    -> void;
auto split_path(const std::string &path) -> std::vector<std::string>;
auto find_executable_in_path(const std::string &command) -> std::string;
auto is_executable(const std::string &filepath) -> bool;
auto execute_command(const std::string &command, const std::string &args,
                     RedirectionSpec redirection_spec) -> bool;
auto redirect_stdout(const std::string &filename) -> bool;
auto redirect_stderr(const std::string &filename) -> bool;

auto main() -> int {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  repl_loop();
  return 0;
}

auto print_prompt() -> void { std::cout << "$ "; }

auto repl_loop() -> void {
  while (true) {
    print_prompt();

    std::string input;
    if (!std::getline(std::cin, input)) {
      break;
    }

    if (!handle_input(input)) {
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

  // Stderr (2>)
  // NOTE(abi): process stderr first to skip over any '>' that's part of '2>'.
  size_t stderr_len = 2;
  size_t stderr_pos = args.find("2>");
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

  // Stdout (> or 1>)
  size_t stdout_length = 0;
  size_t stdout_pos = args.find("1>");
  if (stdout_pos != std::string::npos) {
    stdout_length = 2;
  } else {
    size_t pos = 0;
    while ((pos = args.find(">", pos)) != std::string::npos) {
      if (pos > 0 && args[pos - 1] == '2') {
        pos++;
        continue;
      }
      stdout_pos = pos;
      stdout_length = 1;
      break;
    }
  }

  if (stdout_pos != std::string::npos) {
    size_t filename_start = stdout_pos + stdout_length;
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

  if (stdout_pos != std::string::npos && stderr_pos != std::string::npos) {
    if (stdout_pos > stderr_pos) {
      args = args.substr(0, stdout_pos);
      args = args.substr(0, args.find("2>"));
    } else {
      args = args.substr(0, stderr_pos);
      stdout_pos = args.find("1>");
      if (stdout_pos == std::string::npos) {
        stdout_pos = args.find(">");
      }
      args = args.substr(0, stdout_pos);
    }

  } else if (stdout_pos != std::string::npos) {
    args = args.substr(0, stdout_pos);
  } else if (stderr_pos != std::string::npos) {
    args = args.substr(0, stderr_pos);
  }

  return redirection_spec;
}

auto handle_input(const std::string &input) -> bool {
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

auto find_executable_in_path(const std::string &command) -> std::string {
  const char *path_cstr = std::getenv("PATH");
  if (path_cstr == nullptr) {
    return "";
  }

  std::string path(path_cstr);
  std::vector<std::string> directories = split_path(path);

  for (const std::string &dir : directories) {
    std::string filepath = dir + "/" + command;
    if (is_executable(filepath)) {
      return filepath;
    }
  }

  return "";
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
        if (!redirect_stdout(redirection_spec.stdout_filename)) {
          exit(1);
        }
      }

      if (redirection_spec.has_stderr_redirection) {
        if (!redirect_stderr(redirection_spec.stderr_filename)) {
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

auto redirect_stdout(const std::string &filename) -> bool {
  int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return false;
  }

  int duplicate_fd = dup2(fd, STDOUT_FILENO);
  if (duplicate_fd == -1) {
    std::cerr << "Failed to redirect output" << std::endl;
    close(fd);
    return false;
  }

  close(fd);
  return true;
}

auto redirect_stderr(const std::string &filename) -> bool {
  int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return false;
  }

  int duplicate_fd = dup2(fd, STDERR_FILENO);
  if (duplicate_fd == -1) {
    std::cerr << "Failed to redirect error output" << std::endl;
    close(fd);
    return false;
  }

  close(fd);
  return true;
}
