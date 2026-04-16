#include <cassert>
#include <cstring>

#include <rock_base.hpp>

int main() {
  assert(std::strcmp(rock_base::tag(), "rockBase") == 0);
  assert(rock_base::magic() == 0x524b42);
  return 0;
}
