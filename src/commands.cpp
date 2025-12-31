#include "commands.hpp"
#include "constants.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>

#ifdef _WIN32

#include <windows.h>
constexpr char PATH_LIST_SEPARATOR = ';';

#else

#include <dirent.h>
#include <fcntl.h>
#include <readline/history.h>
#include <sys/stat.h>
#include <unistd.h>

constexpr char PATH_LIST_SEPARATOR = ':';

#endif

namespace ash {

const std::unordered_set<std::string> SHELL_BUILTINS = {
    "exit", "echo", "type", "pwd", "cd", "history"};

std::string previous_directory;
std::vector<std::string> command_history;
size_t command_history_last_write_index = 0;

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
  char cwd[config::MAX_PATH_LENGTH];
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

  char current_dir[config::MAX_PATH_LENGTH];
  if (getcwd(current_dir, sizeof(current_dir)) != nullptr) {
    if (chdir(target_path.c_str()) != 0) {
      std::cout << "cd: " << path << ": No such file or directory" << std::endl;
    } else {
      previous_directory = current_dir;
    }
  }
}

auto load_history_from_file(const std::string &filepath) -> bool {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }

    command_history.push_back(line);
    ::add_history(line.c_str());
  }

  file.close();
  command_history_last_write_index = command_history.size();

  return true;
}

auto write_history_to_file(const std::string &filepath, bool append) -> bool {
  std::ofstream file(filepath, append ? std::ios::app : std::ios::out);
  if (!file.is_open()) {
    return false;
  }

  if (append) {
    for (size_t i = command_history_last_write_index;
         i < command_history.size(); i++) {
      file << command_history[i] << std::endl;
    }
  } else {
    for (const std::string &cmd : command_history) {
      file << cmd << std::endl;
    }
  }

  file.close();
  command_history_last_write_index = command_history.size();

  return true;
}

auto history_command(const std::string &args) -> void {
  if (args.find("-r") == 0 || args.find("-w") == 0 || args.find("-a") == 0) {
    bool read_mode = (args.find("-r") == 0);
    bool append_mode = read_mode ? false : (args.find("-a") == 0);

    auto filename = extract_filename_from_arguments(args, 2);
    if (!filename.has_value()) {
      if (read_mode) {
        std::cerr << "history: -r requires a filename" << std::endl;
        return;
      }

      std::cerr << "history: " << (append_mode ? "-a" : "-w")
                << " requires a filename" << std::endl;
      return;
    }

    if (read_mode) {
      if (!load_history_from_file(*filename)) {
        std::cerr << "history: cannot open " << *filename << std::endl;
      }
    } else {
      if (!write_history_to_file(*filename, append_mode)) {
        std::cerr << "history: cannot open " << *filename << std::endl;
      }
    }

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

auto get_histfile() -> std::optional<std::string> {
  const char *histfile = std::getenv("HISTFILE");
  if (histfile == nullptr) {
    return std::nullopt;
  }

  return std::string(histfile);
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
#endif

auto redirect_stream(StandardStream stream_file_descriptor,
                     const std::string &filename, RedirectionMode mode)
    -> bool {
  int fd = open(filename.c_str(), get_redirection_file_descriptor_flags(mode),
                permissions::DEFAULT_FILE_MODE);
  if (fd == -1) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return false;
  }

  int duplicate_fd = dup2(fd, static_cast<int>(stream_file_descriptor));
  if (duplicate_fd == -1) {
    std::cerr << "Failed to redirect output" << std::endl;
    close(fd);
    return false;
  }

  close(fd);
  return true;
}

auto get_redirection_file_descriptor_flags(RedirectionMode mode) -> int {
  int flags = O_WRONLY | O_CREAT;
  flags |= (mode == RedirectionMode::APPEND) ? O_APPEND : O_TRUNC;

  return flags;
}

} // namespace ash
