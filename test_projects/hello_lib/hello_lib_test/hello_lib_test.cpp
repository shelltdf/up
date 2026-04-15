#include "../hello_lib/hello_lib.hpp"

#include <cassert>

int main() {
  using namespace hello_lib;

  assert(version() == "0.1.0");
  assert(normalize_name("HeLLo") == "hello");
  assert(add(2, 3) == 5);

  Counter counter(1);
  assert(counter.increment() == 2);
  assert(counter.value() == 2);

  Greeter greeter("test");
  assert(greeter.greet("user") == "test, user!");

  const auto values = make_range(3);
  assert(values.size() == 3);
  assert(values[0] == 0 && values[1] == 1 && values[2] == 2);

  return 0;
}
