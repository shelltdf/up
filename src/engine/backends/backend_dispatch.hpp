#pragma once

#include "backend_types.hpp"

namespace up {

int run_build_backend(const BuildBackendContext& ctx, int failure_return_code);
int run_test_backend_ctest(const TestBackendContext& ctx);
int run_test_backend_ninja(const TestBackendContext& ctx, int not_found_return_code);
int run_pack_backend(const PackBackendContext& ctx);
int run_configure_backend(const ConfigureBackendContext& ctx);

}  // namespace up
