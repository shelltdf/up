cmake_minimum_required(VERSION 3.20)
project(vendor_stub NONE)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt" DESTINATION share/native_cmake_vendor_test RENAME vendor_proof.txt)
