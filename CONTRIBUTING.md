# Contributing to CADInspect

Thank you for helping improve CADInspect.

1. Fork the repository and create a focused branch from `main`.
2. Describe defects with reproducible steps. Do not attach confidential CAD files;
   prefer a minimal synthetic STEP fixture.
3. Keep comparison semantics fail-closed. A change must never turn incomplete or
   ambiguous evidence into `PASS`.
4. Build and test the full Release configuration:

   ```powershell
   $env:VCPKG_ROOT = 'C:\path\to\vcpkg'
   cmake --preset oss-release
   cmake --build --preset oss-release
   ctest --preset oss-release
   ```

5. Add focused regression tests and update documentation when observable behavior
   changes.
6. Open a pull request explaining scope, evidence, performance impact and known
   limitations. Keep generated binaries, build trees and customer data out of Git.

Security vulnerabilities should follow `SECURITY.md`, not a public issue.
