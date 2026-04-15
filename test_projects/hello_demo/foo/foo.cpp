#include "foo.hpp"

#include <iostream>

void foo_print() {
  std::cout << "[foo lib] foo_print(): hello from static library\n";
}

Foo::Foo(std::string tag) : tag_(std::move(tag)) {}

void Foo::greet() const {
  std::cout << "[foo lib] Foo::greet() tag=\"" << tag_ << "\"\n";
}
