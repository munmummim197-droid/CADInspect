# Third-party notices

CADInspect source is licensed under the MIT License. A binary CADInspect package
also contains dynamically linked third-party libraries, each governed by its own
license. The authoritative license text for the exact resolved package is copied
from vcpkg metadata into `licenses/` by
`scripts/New-StepComparePortablePackage.ps1`.

Direct dependencies declared by `vcpkg.json`:

| Component | Purpose | License family |
|---|---|---|
| Qt 6 | GUI, widgets, OpenGL and deployment tools | LGPL/GPL/commercial multi-license; package uses dynamic linking |
| Open CASCADE Technology | STEP/XCAF, B-Rep and geometric algorithms | LGPL-2.1 with Open CASCADE exception |
| FreeType | Font rendering dependency | FreeType License or GPL |

The resolved graph may also include zlib, libpng, bzip2, Brotli, PCRE2,
double-conversion, md4c, TBB and other transitive components. Do not use this
summary as a substitute for their complete license texts.

Release maintainers must:

1. build from the pinned vcpkg manifest and dynamic `x64-windows` triplet;
2. preserve all generated files under `dist/licenses/` in portable and installer
   distributions;
3. review the generated license set whenever dependencies or the baseline change;
4. comply with source/relocation/relinking obligations applicable to the chosen
   Qt and Open CASCADE licenses.

The icon at `resources/icons/StepCompare.ico` is a project asset supplied by the
CADInspect owner and is distributed under the project license.
