# Fresh-machine preflight — 2026-08-26

All system/security/VM inventory was read-only. No security setting, ACL, ownership,
certificate, trust store, Defender policy, or VM state was changed.

```text
PROJECT_ROOT=D:\DW\StepCompare
GIT_INITIALIZED=True
INITIAL_BRANCH=main

WINDOWS=Windows 11 Pro 25H2 x64, build 26200.9168
CPU=Intel Core i9-14900K, 24 cores / 32 logical processors
RAM_TOTAL=63.75 GiB
RAM_AVAILABLE_AT_PROBE=36.62 GiB
GPU=NVIDIA GeForce RTX 4060 Ti 16 GiB, driver 591.86

DISK_D=NTFS 931.5 GiB, free 141.6 GiB at probe

VISUAL_STUDIO_BUILD_TOOLS=2026 18.9.1
MSVC_TOOLSET=14.51.36231
CL=19.51.36256 x64
WINDOWS_SDK=10.0.26100.0
CMAKE_CTEST=4.3.1-msvc1
NINJA=1.13.2
GIT=2.55.0.windows.3
VCPKG_BUNDLED=2026-05-27

HYPERV_AVAILABLE=True
POWERSHELL_DIRECT_HOST_CAPABILITY=True
POWERSHELL_DIRECT_END_TO_END_VERIFIED=False
```

One existing VM, `AIHung-Windows-Replay`, was in Saved state with AIHung/Replay
checkpoints. It is treated as reserved:

```text
RUNNER_VM_AVAILABLE=False
```

Host policy evidence:

```text
SMART_APP_CONTROL=Off
VERIFIED_AND_REPUTABLE_POLICY_STATE=0
USER_MODE_CODE_INTEGRITY_ENFORCEMENT=0
APPLOCKER_EFFECTIVE_POLICY=empty
CI_EVENT_3077_LAST_30_DAYS=0
```

An unsigned C++20 x64 probe was built with the installed MSVC and executed with exit
code 0. Therefore:

```text
HOST_UNSIGNED_DEV_EXECUTION_SUITABLE=True
```

The Codex host process exposes both `Path` and `PATH`. MSBuild 18 rejects the duplicate
case variants with `MSB6001`. `scripts/Invoke-StepCompareBuild.ps1` normalizes the
environment only for spawned build processes; it does not alter machine or user
environment variables.

