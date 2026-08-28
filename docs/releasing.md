# Release and signing readiness

Generated executables, DLLs, PDBs, archives and installers are not source files and
must remain outside Git history.

## Proposed signed-release chain

1. Start from a clean, reviewed `main` revision and pinned `vcpkg.json` baseline.
2. Build and run `ctest` using the Release configuration.
3. Package only the CADInspect-owned GUI and CLI PE files as the first future
   SignPath request artifact.
4. Retrieve and verify the signed CADInspect-owned executables.
5. Assemble `dist/` from those executables plus unsigned upstream runtime files;
   review `manifest-sha256.json` and `licenses/`.
6. Build the Inno Setup installer from `installer/CADInspect.iss` using the signed
   CADInspect executable and the unsigned third-party runtime files.
7. Submit the project-owned outer installer as a second future SignPath request.
8. Verify final Authenticode signatures and SHA-256 values on a clean Windows
   machine.
9. Only then publish a release that claims signed status, using the approved tag
   and installer filename for that release.

No release, upload or signing service is invoked by the current build workflow.
The current workflow also does not upload a signing artifact. A future integration
must add an origin-bound artifact handoff after successful Release tests and must
not accept replacement binaries from a maintainer workstation.

The two requests above avoid applying the CADInspect project certificate to Qt,
Open CASCADE, Microsoft runtime, or other upstream DLLs. Whether a provider can
safely deep-sign a particular installer format must be verified before enabling
that feature; the design does not assume deep signing.

## SignPath / OSS signing

The source layout, MIT license, pinned dependency baseline, Release tests and hash
manifest make future OSS signing integration feasible. Readiness remains
**PARTIAL**: the project has no existing binary release in the form to be signed,
GUI/CLI PE metadata is incomplete, the trusted workflow has no artifact handoff,
and SignPath Foundation approval is pending. Do not use a self-signed certificate
for public distribution and do not ask users to weaken Windows Application
Control.

See the [Code signing policy](CODE_SIGNING_POLICY.md), [Privacy policy](PRIVACY.md),
and [Initial release checklist](INITIAL_RELEASE_CHECKLIST.md).

Version is currently `0.1.0` in CMake, the package manifest, and the installer,
while `vcpkg.json` identifies development source as `0.1.0-dev`. Before the first
release, the Owner must decide and synchronize the release version. Windows
numeric PE versions should use the corresponding four-part form, for example
`0.1.0.0` for release `0.1.0`. This documentation task does not change versions.

Before a signing application, add complete VERSIONINFO resources to the GUI and
CLI and obtain an explicit metadata decision for `CompanyName`/publisher and
copyright values. Do not infer a personal or legal identity. The installer
currently uses product `CADInspect`, version `0.1.0.0`, and company `AIHung`, but
lacks copyright and original-filename fields; these values require Owner and
provider review.

## Initial unsigned release

To satisfy the requirement that the project already be released in the form that
will later be signed, prefer a normal unsigned GitHub Release containing the same
portable/installer shapes intended for signing. Publish it only after the metadata
and version decisions above, label it `SIGNING STATUS: UNSIGNED`, provide hashes,
and warn that Windows may show unknown-publisher or reputation prompts.

Under the current `SECURITY.md`, that eligibility release is not an official
trusted release because it has no verified Authenticode signature. Release notes
must call it an unsigned eligibility release; the Owner must explicitly reconcile
the official-release policy before public publication.

A pre-release may be useful for testing but provides weaker evidence that the
project is already released in its intended stable form. A source-only release
does not demonstrate the binary form intended for signing. No release is created
by this documentation task.
