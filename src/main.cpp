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
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

constexpr char PATH_LIST_SEPARATOR = ':';
#endif

const std::unordered_set<std::string> SHELL_BUILTINS = {"exit", "echo", "type",
                                                        "pwd"};

auto print_prompt() -> void;
auto handle_input(const std::string &input) -> bool;
auto handle_invalid_input(const std::string &input) -> void;
auto repl_loop() -> void;
auto is_builtin(const std::string &command) -> bool;
auto echo_command(const std::string &args) -> void;
auto type_command(const std::string &name) -> void;
auto pwd_command() -> void;
auto split_path(const std::string &path) -> std::vector<std::string>;
auto find_executable_in_path(const std::string &command) -> std::string;
auto is_executable(const std::string &filepath) -> bool;
auto parse_arguments(const std::string &args) -> std::vector<std::string>;
auto execute_command(const std::string &command,
                     const std::vector<std::string> &args) -> void;

auto main() -> int {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  repl_loop();
  return 0;
}

auto print_prompt() -> void { std::cout << "$ "; }

auto handle_input(const std::string &input) -> bool {
  size_t first_space_pos = input.find(' ');
  std::string command = input.substr(0, first_space_pos);
  std::string args = (first_space_pos != std::string::npos)
                         ? input.substr(first_space_pos + 1)
                         : "";

  if (command == "exit") {
    return false;
  }

  if (command == "echo") {
    echo_command(args);
  } else if (command == "type") {
    type_command(args);
  } else if (command == "pwd") {
    pwd_command();
  } else if (!find_executable_in_path(command).empty()) {
    execute_command(command, parse_arguments(args));
  } else {
    handle_invalid_input(input);
  }

  return true;
}

auto handle_invalid_input(const std::string &input) -> void {
  std::cout << input << ": command not found" << std::endl;
}

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

auto is_builtin(const std::string &command) -> bool {
  return SHELL_BUILTINS.find(command) != SHELL_BUILTINS.end();
}

auto echo_command(const std::string &args) -> void {
  std::cout << args << std::endl;
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

auto execute_command(const std::string &command,
                     const std::vector<std::string> &args) -> void {
  pid_t pid = fork();
  if (pid == -1) {
    std::cerr << "Failed to fork process" << std::endl;
    return;
  }

  // Child process
  if (pid == 0) {
    // Program name + arguments
    // NOTE(abi): it needs to be null-terminated.
    std::vector<char *> c_args;
    c_args.push_back(const_cast<char *>(command.c_str()));
    for (const auto &arg : args) {
      c_args.push_back(const_cast<char *>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    execvp(command.c_str(), c_args.data());

    std::cerr << command << ": command not found" << std::endl;
    exit(1);
  }
  // Parent process
  else {
    int status;
    waitpid(pid, &status, 0);
  }
}
#endif

auto parse_arguments(const std::string &args) -> std::vector<std::string> {
  std::stringstream ss(args);
  std::vector<std::string> parsed_args;
  std::string argument;

  while (std::getline(ss, argument, ' ')) {
    parsed_args.push_back(argument);
  }

  return parsed_args;
}
