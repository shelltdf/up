#pragma once

#include <string>
#include <vector>

namespace hello_simple_lib {

std::string version();
std::string normalize_name(std::string name);
int add(int a, int b);

class Counter {
 public:
  explicit Counter(int initial = 0) : value_(initial) {}
  int increment();
  int value() const { return value_; }

 private:
  int value_{0};
};

class Greeter {
 public:
  explicit Greeter(std::string prefix) : prefix_(std::move(prefix)) {}
  std::string greet(const std::string& who) const;

 private:
  std::string prefix_;
};

std::vector<int> make_range(int n);

}  // namespace hello_simple_lib
