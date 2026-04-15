#include <foo.hpp>
#include <hello_lib.hpp>

#include <iostream>

int main() {
  std::cout << "[hello_demo] main()\n";
  foo_print();
  Foo foo("hello_demo");
  foo.greet();
  std::cout << "[hello_demo] hello_lib::version=" << hello_lib::version() << "\n";
  std::cout << "[hello_demo] hello_lib::add(7, 5)=" << hello_lib::add(7, 5) << "\n";
  hello_lib::Greeter greeter("hello_demo");
  std::cout << "[hello_demo] " << greeter.greet("cross_package") << "\n";
  return 0;
}
