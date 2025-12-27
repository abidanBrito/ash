#include <iostream>
#include <string>

void print_prompt();
bool handle_input(const std::string &input);
void handle_invalid_input(const std::string &input);
void repl_loop();

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  repl_loop();

  return 0;
}

void print_prompt() { std::cout << "$ "; }

bool handle_input(const std::string &input) {
  if (input == "exit") {
    return false;
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
