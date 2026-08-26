# StepCompare DEV V1

StepCompare is a fresh native Windows application for comparing STEP/STP parts and
assemblies. The comparison core is independent of the Qt user interface and is
shared by `StepCompare.exe` and `stepcompare-cli.exe`.

The project is being built in evidence-backed phases. A probable match is never
reported as `PASS`; incomplete or ambiguous evidence is reported as `CHECK`.

## Current status

Phase 0 foundation is in progress. The current build contains the dependency-free
domain model and tests. Qt 6 and OpenCASCADE adapters are enabled only after their
versions and vcpkg baseline pass an MSVC x64 compatibility build.

## Core-only build

The Codex host currently exposes duplicate case variants of `Path`/`PATH`, which
MSBuild rejects. The checked-in wrapper normalizes that child-process environment
without modifying the machine:

```powershell
.\scripts\Invoke-StepCompareBuild.ps1 -Stage All
```

Default tolerances are 0.01 mm for position/surface and 0.01 degrees for angle.
Absolute translation follows `Delta = File B - File A`. Alignment transforms are
always named and represented as `B -> A`.
