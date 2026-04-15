#include "../hello_lib/hello_lib.hpp"

#include <iostream>

int main() {
  using namespace hello_lib;

  Counter counter(5);
  counter.increment();

  Greeter greeter("hello_lib_tool");
  std::cout << greeter.greet(normalize_name("WORLD")) << "\n";
  std::cout << "version=" << version() << ", sum=" << add(10, 20) << ", counter=" << counter.value() << "\n";

  const auto values = make_range(4);
  std::cout << "range:";
  for (int v : values)
    std::cout << " " << v;
  std::cout << "\n";
  return 0;
}
