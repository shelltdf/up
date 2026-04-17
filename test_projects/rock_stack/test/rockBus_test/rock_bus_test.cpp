#include <cassert>
#include <cstring>

#include <rock_bus.hpp>

int main() {
  assert(std::strcmp(rock_bus::tag(), "rockBus") == 0);
  assert(rock_bus::slots() == 8);
  return 0;
}
