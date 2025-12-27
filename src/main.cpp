#include <iostream>
#include <string>

void print_prompt() { std::cout << "$ "; }

void handle_invalid_input(const std::string &input) {
  std::cout << input << ": command not found" << std::endl;
}

void repl_loop() {
  while (true) {
    print_prompt();

    std::string command;
    if (!std::getline(std::cin, command)) {
      break;
    }

    handle_invalid_input(command);
  }
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  repl_loop();

  return 0;
}
