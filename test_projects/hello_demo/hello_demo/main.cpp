// -----------------------------------------------------------------------------
// hello_demo: hello_foo (local), hello_simple_lib + rock_stack (cross-package smoke test)
// -----------------------------------------------------------------------------

#include <iostream>

#include <foo.hpp>
#include <hello_simple_lib.hpp>
#include <parent_lib.hpp>
#include <rock_base.hpp>
#include <rock_bus.hpp>
#include <rock_net.hpp>

int main() {
  std::cout << "[hello_demo] main()\n";

  // --- hello_foo (this package): foo.hpp / Foo --------------------------------
  foo_print();
  Foo foo("hello_demo");
  foo.greet();

  // --- hello_simple_lib (cross-package) ----------------------------------------
  std::cout << "[hello_demo] hello_simple_lib::version=" << hello_simple_lib::version() << "\n";
  std::cout << "[hello_demo] hello_simple_lib::add(7, 5)=" << hello_simple_lib::add(7, 5) << "\n";
  hello_simple_lib::Greeter greeter("hello_demo");
  std::cout << "[hello_demo] " << greeter.greet("cross_package") << "\n";

  // --- hello_parent_child: parent_lib (cross-package) --------------------------
  std::cout << "[hello_demo] parent_lib::tag()=" << parent_lib::tag()
            << " answer=" << parent_lib::answer() << "\n";

  // --- rock_stack: rockBase / rockNet / rockBus (headers under rock_stack/include/...)
  std::cout << "[hello_demo] rock_base::tag()=" << rock_base::tag() << " magic=0x" << std::hex
            << rock_base::magic() << std::dec << "\n";
  std::cout << "[hello_demo] rock_net::tag()=" << rock_net::tag() << " port=" << rock_net::port() << "\n";
  std::cout << "[hello_demo] rock_bus::tag()=" << rock_bus::tag() << " slots=" << rock_bus::slots() << "\n";

  return 0;
}
