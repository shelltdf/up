#include <cstdio>
#include "stub.h"

int main() {
  std::printf("prebuilt_stub_value=%d\n", prebuilt_stub_value());
  return prebuilt_stub_value() == 42 ? 0 : 1;
}
