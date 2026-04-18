#include "../hello_simple_lib/hello_simple_lib.hpp"
#include "hello_simple_lib_gen.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
  using namespace hello_simple_lib;

  std::cout << "gen: TEST_PACKAGE_NAME=" << gen::kTestPackageName << "\n";
  std::cout << "gen: TEST_TARGET_NAME=" << gen::kTestTargetName << "\n";
  std::cout << "gen: TEST_BUILD_SYSTEM=" << gen::kTestBuildSystem << "\n";
  std::cout << "gen: TEST_TEMPLATE_TAG=" << gen::kTestTemplateTag << "\n";

  assert(std::string(gen::kTestPackageName) == "hello_simple_lib");
  assert(std::string(gen::kTestTargetName) == "hello_simple_lib");
  assert(std::string(gen::kTestBuildSystem) == "cmake");
  // TEST_TEMPLATE_TAG: package.xml 默认 -> target.xml 覆盖；还可由 --opt / up_cache 再覆盖。
  assert(std::string(gen::kTestTemplateTag) == "from_target_xml");
  assert(package_config_tag() == "from_package_xml");

#if !defined(HELLO_SIMPLE_LIB_FROM_PACKAGE) || (HELLO_SIMPLE_LIB_FROM_PACKAGE != 1)
#error "expected package.xml <defines> HELLO_SIMPLE_LIB_FROM_PACKAGE=1"
#endif

  assert(version() == "0.1.0");
  assert(normalize_name("HeLLo") == "hello");
  assert(add(2, 3) == 5);

  Counter counter(1);
  assert(counter.increment() == 2);
  assert(counter.value() == 2);

  Greeter greeter("test");
  assert(greeter.greet("user") == "test, user!");

  const auto values = make_range(3);
  assert(values.size() == 3);
  assert(values[0] == 0 && values[1] == 1 && values[2] == 2);

  return 0;
}
