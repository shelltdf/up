#include "hello_lib.hpp"

#include <algorithm>
#include <cctype>

namespace hello_lib {

std::string version() { return "0.1.0"; }

std::string normalize_name(std::string name) {
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return name;
}

int add(int a, int b) { return a + b; }

int Counter::increment() { return ++value_; }

std::string Greeter::greet(const std::string& who) const { return prefix_ + ", " + who + "!"; }

std::vector<int> make_range(int n) {
  std::vector<int> out;
  if (n <= 0)
    return out;
  out.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i)
    out.push_back(i);
  return out;
}

}  // namespace hello_lib
