// Unit tests for foo; run via CTest / `up test`.

#include "../foo/foo.hpp"

#include <cassert>

int main() {
  foo_print();

  Foo f("unit_test");
  assert(f.tag() == "unit_test");
  f.greet();

  const Foo g("x");
  assert(g.tag() == "x");

  return 0;
}
