# Initial release checklist

Use this checklist for the first public CADInspect binary release. It prepares
evidence only; completing the document does not itself create a tag, release, or
signing request.

## Identity and source

- [ ] CADInspect version: `________________`
- [ ] Release commit SHA: `________________________________________`
- [ ] GitHub Actions run URL/ID: `________________`
- [ ] Repository and worktree were clean at the recorded commit.
- [ ] Release diff, build scripts, and workflow changes were reviewed.

## Build and tests

- [ ] GitHub-hosted Release build: `PASS / FAIL`
- [ ] Automated tests: `____ / ____ PASS`
- [ ] Source-only policy: `PASS / FAIL`
- [ ] Release-only dependency check: `PASS / FAIL`
- [ ] No manual binary or source injection occurred after checkout.

## Artifact identity

- [ ] GUI executable SHA-256: `________________`
- [ ] CLI executable SHA-256, if distributed: `________________`
- [ ] Portable package SHA-256: `________________`
- [ ] Installer filename: `________________`
- [ ] Installer SHA-256: `________________`
- [ ] Package manifest SHA-256: `________________`
- [ ] Product, file, and original-filename PE metadata verified.

## Signing status

Record exactly one status and do not imply trust that has not been verified:

- [ ] `SIGNING STATUS: UNSIGNED`
- [ ] `SIGNING STATUS: SIGNED — Authenticode verified`

For a signed release, record the signer subject, timestamp status, SignPath
request identity, and Authenticode verification output. For an unsigned release,
state prominently on the download page that Windows may display an unknown
publisher or reputation warning.

## Ownership boundary and notices

- [ ] Only CADInspect-owned executables were submitted for project signing.
- [ ] Qt, Open CASCADE, Microsoft runtime, and other upstream binaries were not
  signed using the CADInspect project certificate.
- [ ] `LICENSE`, `THIRD_PARTY_NOTICES.md`, and the generated `licenses/` tree are
  present in the distributed package.
- [ ] The exact resolved third-party notice set was reviewed.

## Installation evidence

- [ ] Clean-machine installation completed.
- [ ] Program Files, Start Menu, optional desktop shortcut, and App Paths changes
  matched the installer definition.
- [ ] No unexpected auto-launch or network communication occurred.
- [ ] Windows uninstallation completed and removed the registered application.

## Publication gate

- [ ] Release notes link the [Code signing policy](CODE_SIGNING_POLICY.md),
  [Privacy policy](PRIVACY.md), and third-party notices.
- [ ] Signing status and SHA-256 values are visible beside downloads.
- [ ] Final files match the hashes recorded above.
- [ ] A human approver authorized publication.

## Initial release strategy

The recommended eligibility release is a normal unsigned GitHub Release in the
same package and installer form intended for future signing, after version and PE
metadata blockers are resolved. It should be explicitly labelled
`SIGNING STATUS: UNSIGNED` and include SHA-256 values and all notices.
Under `SECURITY.md`, it must also be described as a non-official eligibility
release; only a checksum-published release with a verified trusted Authenticode
signature qualifies as official and trusted.

A GitHub pre-release reduces expectations but may not demonstrate that the
project is already released in the final form SignPath Foundation expects. A
source-only release does not demonstrate the executable/installer form intended
for signing. The normal unsigned release therefore provides clearer evidence,
at the cost of Windows unknown-publisher and reputation warnings until signing is
approved.
