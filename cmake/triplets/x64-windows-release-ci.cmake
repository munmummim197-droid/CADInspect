# CI-only counterpart of vcpkg's dynamic x64-windows triplet. Local developer
# presets intentionally continue to use the standard dual-configuration triplet.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_PROVIDED_FORTRAN ON)
