#include <iostream>
#include <string>

#include <rock_base.hpp>
#include <rock_bus.hpp>
#include <rock_net.hpp>

// rock_app_two: second entry point; same libs, different output.

int main() {
  const std::string line = std::string(rock_base::tag()) + " | " + rock_net::tag() + " | " + rock_bus::tag();
  std::cout << "[rock_app_two] " << line << "\n";
  std::cout << "[rock_app_two] sum_meta=" << (rock_base::magic() + static_cast<int>(rock_net::port()) + rock_bus::slots())
            << "\n";
  return 0;
}
