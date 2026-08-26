# Dependency baseline

DEV V1 pins the vcpkg builtin registry to:

```text
f89a4a1da4e3176a8d1a14c1825b9b2f98e48843
```

The selected versions are Qt `6.11.1#1` and OpenCASCADE `8.0.1`. Qt uses only
`Core`, `Gui`, `Widgets`, `Concurrent`, `OpenGL/OpenGLWidgets`, threading, and the
Windows deployment tool path required by the desktop application. OCCT keeps only
the FreeType feature beyond mandatory port dependencies.

The triplet is dynamic `x64-windows`, which supports a portable DLL distribution and
avoids making static Qt linkage the default licensing decision.

MSVC v145 is installed, while current Qt/OCCT vendor matrices list Visual Studio 2022.
Both dependencies were built from source in Debug and Release, followed by a real
API smoke covering Qt Core, OCCT STEP/XCAF import, meshing, volume/bounds, and a
Unicode Windows path. Host compatibility for the selected DEV configuration is now:

```text
DEPENDENCY_COMPATIBILITY_CONFIRMED=True
```

This is host evidence, not a claim that v145 is in each vendor's certified matrix.
An AIS/OpenGL physical-preview test remains a separate Phase 2 gate. If a later
vendor-supported configuration requires v143, installing it side-by-side requires
Owner approval because it changes the machine toolchain.
