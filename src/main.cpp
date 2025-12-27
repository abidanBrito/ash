#include <iostream>
#include <string>

void print_prompt();
bool handle_input(const std::string &input);
void handle_invalid_input(const std::string &input);
void repl_loop();
void echo_command(const std::string &args);

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

  if (command == "exit") {
    return false;
  }

  if (command == "echo") {
    std::string args = (first_space_pos != std::string::npos)
                           ? input.substr(first_space_pos + 1)
                           : "";
    echo_command(args);
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

void echo_command(const std::string &args) { std::cout << args << std::endl; }
