#include <hello_parent_child_child_lib.hpp>

#include <iostream>

int main() {
  std::cout << "[hello_parent_child_app] child id=" << hello_parent_child_child::id() << "\n";
  std::cout << "[hello_parent_child_app] " << hello_parent_child_child::greet("parent") << "\n";
  return 0;
}
