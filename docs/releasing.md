# Release and signing readiness

Generated executables, DLLs, PDBs, archives and installers are not source files and
must remain outside Git history.

## Proposed release chain

1. Start from a clean, reviewed `main` revision and pinned `vcpkg.json` baseline.
2. Build and run `ctest` using the Release configuration.
3. Generate `dist/`; review `manifest-sha256.json` and `licenses/`.
4. Build the Inno Setup installer from `installer/CADInspect.iss`.
5. Sign project executables and the installer with a trusted Authenticode provider.
6. Verify signatures and hashes on a clean Windows machine.
7. Only then create a tag such as `v0.1.0` and publish
   `CADInspect-Setup-x64-0.1.0.exe` through the repository release page.

No release, upload or signing service is invoked by the current build workflow.

## SignPath / OSS signing

The source layout, public-license intent, pinned dependencies, Release tests and
hash manifest make future OSS signing integration feasible. Readiness is currently
**PARTIAL**: a public remote, transparent release workflow, provider approval and
trusted certificate identity still need to be selected. Do not use a self-signed
certificate for public distribution and do not ask users to weaken Windows
Application Control.

Version is currently `0.1.0` in CMake and the installer. Version changes require a
separate release decision; they must remain synchronized.
