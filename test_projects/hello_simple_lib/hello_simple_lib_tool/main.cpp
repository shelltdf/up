#include "../hello_simple_lib/hello_simple_lib.hpp"
#include "hello_simple_lib_gen.hpp"

#include <iostream>

int main() {
  using namespace hello_simple_lib;

  std::cout << "gen: TEST_PACKAGE_NAME=" << gen::kTestPackageName << "\n";
  std::cout << "gen: TEST_TARGET_NAME=" << gen::kTestTargetName << "\n";
  std::cout << "gen: TEST_BUILD_SYSTEM=" << gen::kTestBuildSystem << "\n";
  std::cout << "gen: TEST_TEMPLATE_TAG=" << gen::kTestTemplateTag << "\n";
  std::cout << "pkg: package_config_tag=" << package_config_tag() << "\n";

  Counter counter(5);
  counter.increment();

  Greeter greeter("hello_simple_lib_tool");
  std::cout << greeter.greet(normalize_name("WORLD")) << "\n";
  std::cout << "version=" << version() << ", sum=" << add(10, 20) << ", counter=" << counter.value() << "\n";

  const auto values = make_range(4);
  std::cout << "range:";
  for (int v : values)
    std::cout << " " << v;
  std::cout << "\n";
  return 0;
}
