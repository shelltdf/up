#include <cassert>

#include <rock_net.hpp>

int main() {
  assert(rock_net::port() == 9955u);
  return 0;
}
