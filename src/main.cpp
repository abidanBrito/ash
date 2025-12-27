#include <iostream>
#include <string>
#include <unordered_set>

const std::unordered_set<std::string> SHELL_BUILTINS = {"exit", "echo", "type"};

void print_prompt();
bool handle_input(const std::string &input);
void handle_invalid_input(const std::string &input);
void repl_loop();
bool is_builtin(const std::string &command);
void echo_command(const std::string &args);
void type_command(const std::string &name);

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  repl_loop();

  return 0;
}

void print_prompt() { std::cout << "$ "; }

bool handle_input(const std::string &input) {
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
    return true;
  }

  if (command == "type") {
    type_command(args);
    return true;
  }

  handle_invalid_input(input);
  return true;
}

void handle_invalid_input(const std::string &input) {
  std::cout << input << ": command not found" << std::endl;
}

void repl_loop() {
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

bool is_builtin(const std::string &command) {
  return SHELL_BUILTINS.find(command) != SHELL_BUILTINS.end();
}

void echo_command(const std::string &args) { std::cout << args << std::endl; }

void type_command(const std::string &name) {
  if (is_builtin(name)) {
    std::cout << name << " is a shell builtin" << std::endl;
  } else {
    std::cout << name << ": not found" << std::endl;
  }
}
