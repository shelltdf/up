#include <cstdio>
#include "tiny.h"
int main() {
  std::printf("tiny_add(2,3)=%d\n", tiny_add(2, 3));
  return tiny_add(2, 3) == 5 ? 0 : 1;
}
