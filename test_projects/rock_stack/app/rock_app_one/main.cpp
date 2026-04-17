#include <iostream>

#include <rock_base.hpp>
#include <rock_bus.hpp>
#include <rock_net.hpp>

// rock_app_one: uses rockBase, rockNet, rockBus (exe links all static libs in package).

int main() {
  std::cout << "[rock_app_one] rock_base::tag()=" << rock_base::tag() << " magic=0x" << std::hex
            << rock_base::magic() << std::dec << "\n";
  std::cout << "[rock_app_one] rock_net::tag()=" << rock_net::tag() << " port=" << rock_net::port() << "\n";
  std::cout << "[rock_app_one] rock_bus::tag()=" << rock_bus::tag() << " slots=" << rock_bus::slots() << "\n";
  return 0;
}
