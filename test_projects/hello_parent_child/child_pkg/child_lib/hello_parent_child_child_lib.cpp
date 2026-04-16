#include "hello_parent_child_child_lib.hpp"

namespace hello_parent_child_child {

int id() { return 42; }

std::string greet(const std::string& from) { return "hello from child package to " + from; }

}  // namespace hello_parent_child_child
