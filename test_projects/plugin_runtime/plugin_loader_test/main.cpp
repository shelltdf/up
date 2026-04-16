#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

using FnInt = int (*)();
using FnInfo = const char* (*)();

int main(int argc, char** argv) {
  const fs::path exe_path = (argc > 0 && argv[0] != nullptr) ? fs::absolute(argv[0]) : fs::current_path();
  const fs::path bin_dir = exe_path.parent_path();

#ifdef _WIN32
  fs::path plugin_path = bin_dir / "plugin_lib.dll";
  HMODULE handle = LoadLibraryA(plugin_path.string().c_str());
  if (!handle) {
    // Fallback for `up run`: exe is under .intermediate/install/<arch>/bin,
    // while plugin dll is currently under .intermediate/build/<arch>/out/Release.
    const fs::path arch_dir = bin_dir.parent_path();
    const fs::path install_dir = arch_dir.parent_path();
    if (!install_dir.empty() && install_dir.filename() == "install") {
      plugin_path = install_dir.parent_path() / "build" / arch_dir.filename() / "out" / "Release" / "plugin_lib.dll";
      handle = LoadLibraryA(plugin_path.string().c_str());
    }
  }
  if (!handle) {
    std::cerr << "plugin_loader_test: failed to load " << plugin_path.string() << "\n";
    return 2;
  }

  auto init_fn = reinterpret_cast<FnInt>(GetProcAddress(handle, "init"));
  auto update_fn = reinterpret_cast<FnInt>(GetProcAddress(handle, "update"));
  auto shutdown_fn = reinterpret_cast<FnInt>(GetProcAddress(handle, "shutdown"));
  auto info_fn = reinterpret_cast<FnInfo>(GetProcAddress(handle, "info"));
#else
  const fs::path plugin_path = bin_dir / "libplugin_lib.so";
  void* handle = dlopen(plugin_path.string().c_str(), RTLD_NOW);
  if (!handle) {
    std::cerr << "plugin_loader_test: failed to load " << plugin_path.string() << "\n";
    return 2;
  }

  auto init_fn = reinterpret_cast<FnInt>(dlsym(handle, "init"));
  auto update_fn = reinterpret_cast<FnInt>(dlsym(handle, "update"));
  auto shutdown_fn = reinterpret_cast<FnInt>(dlsym(handle, "shutdown"));
  auto info_fn = reinterpret_cast<FnInfo>(dlsym(handle, "info"));
#endif

  if (!(init_fn && update_fn && shutdown_fn && info_fn)) {
    std::cerr << "plugin_loader_test: missing required plugin exports\n";
#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
    return 3;
  }

  const int init_rc = init_fn();
  const int update_rc = update_fn();
  const int shutdown_rc = shutdown_fn();
  const char* info_txt = info_fn();

  std::cout << "[plugin_loader_test] info=" << (info_txt ? info_txt : "<null>") << "\n";
  std::cout << "[plugin_loader_test] init=" << init_rc << " update=" << update_rc
            << " shutdown=" << shutdown_rc << "\n";

#ifdef _WIN32
  FreeLibrary(handle);
#else
  dlclose(handle);
#endif
  return (init_rc == 0 && shutdown_rc == 0) ? 0 : 4;
}
