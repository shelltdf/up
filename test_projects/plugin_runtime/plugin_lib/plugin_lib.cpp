#include <cstdint>

extern "C" {

#ifdef _WIN32
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API
#endif

PLUGIN_API int init() { return 0; }

PLUGIN_API int update() {
  static int tick = 0;
  return ++tick;
}

PLUGIN_API int shutdown() { return 0; }

PLUGIN_API const char* info() { return "plugin_lib v0.1.0"; }

}  // extern "C"
