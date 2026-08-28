# Code signing policy

Code signing for CADInspect is planned and remains pending approval by SignPath
Foundation. No current CADInspect release or binary may be described as signed by
SignPath unless its Authenticode signature has been verified on the exact published
artifact.

Free code signing provided by SignPath.io,
certificate by SignPath Foundation

## Project and team model

CADInspect is currently maintained by one public GitHub identity. No additional
team member is implied by this policy.

- Authors / Committers: `munmummim197-droid`
- Reviewers: `munmummim197-droid`
- Signing approver: `munmummim197-droid`

In this single-maintainer model, the maintainer is responsible for repository
access, reviewing release-bound changes, and manually approving each future
signing request. Multi-factor authentication is required for repository and
future signing-service access. If more maintainers are added, roles and review
separation must be documented before they participate in signing.

## Signed artifacts

Only artifacts built from source and build definitions owned by CADInspect may be
submitted under a future CADInspect signing policy:

- the GUI executable built as `StepCompare.exe` and installed as `CADInspect.exe`;
- `stepcompare-cli.exe` when it is distributed as a project artifact;
- any future CADInspect-owned DLL built from source in this repository; and
- `CADInspect-Setup-x64-<version>.exe`, built from
  `installer/CADInspect.iss`, after the application executables inside it have
  completed their own signing stage.

CADInspect currently builds no project-owned DLL. Its internal libraries are
static libraries linked into the two project executables.

## Unsigned third-party components

Upstream binaries are outside the CADInspect certificate boundary and must not be
submitted for signing with a certificate allocated to CADInspect. This includes:

- Qt DLLs and Qt plugins;
- Open CASCADE Technology DLLs;
- Microsoft Visual C++ runtime DLLs; and
- DLLs from transitive vcpkg dependencies such as FreeType, zlib, libpng, Brotli,
  bzip2, PCRE2, double-conversion, and md4c.

These files may be bundled without a CADInspect signature when their licenses and
notices permit it. Exact notices are generated into `dist/licenses/`; see
[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md). An outer CADInspect
installer signature must not be represented as a project signature on the
upstream files contained inside it.

## Build origin and trusted build

Release candidates must originate from a reviewed commit on the public CADInspect
repository. The tracked GitHub Actions workflow uses a GitHub-hosted Windows
runner, a pinned vcpkg registry baseline, Release-only dependency triplet, build,
automated tests, and a source-only policy check.

Before SignPath integration, the trusted workflow still needs an explicit
artifact packaging and handoff stage that binds each signing request to the
origin commit and GitHub Actions run. No manually supplied or locally rebuilt
binary may replace a workflow artifact after that boundary.

## Release approval and integrity

Every future signing request requires manual approval by the documented signing
approver. Approval must occur only after the Release build and all required tests
pass. Published release records must identify the source commit, GitHub Actions
run, signing status, SHA-256 values, and third-party notices.

Authenticode verification and SHA-256 generation must run against the exact final
files after each signing or assembly stage. A failed, missing, or unverifiable
signature blocks a release that claims to be signed.

## Signing responsibilities

The maintainer is responsible for:

1. protecting GitHub and future SignPath access with multi-factor authentication;
2. reviewing source, build scripts, workflow changes, and dependency changes;
3. approving only artifacts whose origin is verifiable;
4. keeping project-owned and upstream binary signing boundaries separate;
5. verifying final Authenticode signatures and hashes; and
6. ensuring release notes never describe unsigned artifacts as signed or trusted.

## Privacy statement

CADInspect's current runtime does not implement telemetry, analytics, automatic
update checks, crash uploads, external API calls, or user-data uploads. See the
audited [`Privacy policy`](PRIVACY.md).

## Release process

The planned two-stage process is documented in
[`releasing.md`](releasing.md). Release evidence must be completed using the
[`Initial release checklist`](INITIAL_RELEASE_CHECKLIST.md). Signing remains
planned and pending Foundation approval; this policy does not apply for, request,
or imply approval by SignPath Foundation.
