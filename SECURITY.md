# Security policy

## Reporting a vulnerability

Please do not disclose a potentially exploitable vulnerability in a public issue.
Use GitHub's **Security → Advisories → Report a vulnerability** flow for the
CADInspect repository when private vulnerability reporting is enabled. If that
option is unavailable, contact the repository owner privately through the owner
contact shown on GitHub and share only the minimum evidence required.

Include the affected revision, impact, reproduction steps and a synthetic input
when possible. Do not send customer CAD, credentials, private keys or personal
data.

## Scope and safe operation

CADInspect parses STEP/STP through native Qt/OpenCASCADE components in the local
process. Treat CAD files as untrusted and do not run the application with elevated
privileges. Extremely large or crafted models can be resource-intensive; use an
isolated workstation for suspicious inputs.

Canonical JSON/CSV reports contain full input paths, SHA-256 values and model
metadata for traceability. Review or redact them before external sharing.

Supported security fixes target the current `main` branch. Release artifacts are
considered official only after their checksum and trusted Authenticode signature
have been published and verified.
