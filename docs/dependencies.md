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
Therefore the dependency configuration is pinned but not yet declared compatible:

```text
DEPENDENCY_COMPATIBILITY_CONFIRMED=False
```

The flag becomes true only after Qt/OCCT build plus STEP/XCAF import, meshing, and AIS
viewer smoke tests on this host. If v145 fails, installing v143 side-by-side requires
Owner approval because it changes the machine toolchain.

