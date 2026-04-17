# native_cmake_vendor

Demonstrates `package.xml` **`<cmake source_dir="..."/>`**: the upstream directory `vendor_stub/` is built and installed via CMake `ExternalProject` during `up configure` / `up build`, before the `up`-managed executable is linked.

The app target does not consume vendor artifacts; the fixture only verifies that configure and build succeed with an external CMake subtree.
