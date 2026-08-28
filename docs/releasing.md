# Release and signing readiness

Generated executables, DLLs, PDBs, archives and installers are not source files
and must remain outside Git history. The canonical release version is `0.1.0`;
Windows numeric file version is `0.1.0.0`.

## Trusted unsigned build handoff

The tracked GitHub Actions workflow runs on `windows-2022` and performs this
commit-bound sequence:

1. check out the exact source commit;
2. restore the keyed vcpkg binary cache and configure the Release-only triplet;
3. build the GUI and CLI and run the complete Release test suite;
4. enforce the source-only repository policy;
5. assemble the portable runtime and its SHA-256 manifest;
6. build the unsigned Inno Setup installer;
7. stage the exact project-owned PE inputs, portable runtime, installer and a
   provenance manifest containing version, source commit and workflow run ID;
8. upload one deterministic workflow artifact named
   `cadinspect-0.1.0-unsigned-<commit-sha>`.

The workflow artifact is an internal trusted-build handoff, not a GitHub Release
asset and not evidence of an Authenticode signature. GitHub also records a
SHA-256 digest for uploaded workflow artifacts. Nothing in the current workflow
calls SignPath, needs `SIGNPATH_API_TOKEN`, creates a tag, or publishes a release.

## Proposed signed-release chain

1. Start from a clean, reviewed `main` revision and the commit-bound workflow
   artifact described above.
2. Submit only `CADInspect.exe` and `stepcompare-cli.exe` as the first future
   SignPath request inputs.
3. Verify the returned Authenticode signatures and hashes, then replace only
   those two owned files in the portable runtime.
4. Recompute the portable manifest without altering Qt, Open CASCADE, Microsoft
   runtime, or other upstream DLLs.
5. Build a new outer installer from the signed owned executables and unchanged
   upstream runtime.
6. Submit only `CADInspect-Setup-x64-0.1.0.exe` as the second future SignPath
   request input.
7. Verify final signatures and SHA-256 values on a clean Windows machine.
8. Only then publish a release that claims signed status.

No maintainer-workstation binary may replace a GitHub-hosted workflow artifact
after the trusted-build boundary. Whether the signing provider can safely
deep-sign an installer format must be confirmed before enabling such a feature;
this design does not assume deep signing.

## Signing boundary

Sign as CADInspect project artifacts:

- `CADInspect.exe`;
- `stepcompare-cli.exe`;
- any future project-owned DLL; and
- the final outer `CADInspect-Setup-x64-<version>.exe`.

Do not sign as CADInspect project artifacts:

- Qt DLLs or plugins;
- Open CASCADE Technology DLLs;
- Microsoft runtime DLLs; or
- other upstream dependency binaries.

## Initial unsigned release

An initial unsigned `0.1.0` eligibility release may use the same portable and
installer shapes as the trusted-build workflow. It must be labelled
`SIGNING STATUS: UNSIGNED`, identify the source commit and workflow run, publish
SHA-256 values, and warn that Windows may show unknown-publisher or reputation
prompts. It is not an official trusted/signed release under `SECURITY.md`.

SignPath Foundation approval remains pending. Do not describe any current binary
as signed, trusted by SignPath, or approved by SignPath. Do not use a self-signed
certificate for public distribution and do not ask users to weaken Windows
Application Control.

See the [Code signing policy](CODE_SIGNING_POLICY.md),
[Privacy policy](PRIVACY.md), and
[Initial release checklist](INITIAL_RELEASE_CHECKLIST.md).
