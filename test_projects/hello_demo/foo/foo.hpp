#pragma once

#include <string>

void foo_print();

class Foo {
 public:
  explicit Foo(std::string tag);
  void greet() const;
  const std::string& tag() const { return tag_; }

 private:
  std::string tag_;
};
