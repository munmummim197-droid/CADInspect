# ADR 0001: Core boundaries and comparison conventions

Status: Accepted for DEV V1 foundation

## Decision

StepCompare uses ports-and-adapters. Domain, ports, and application targets use
standard C++20 types only. They do not expose Qt or OpenCASCADE types in public
headers. Qt/OCCT implementations are adapters selected by the composition roots of
the GUI and CLI executables.

Internal length is millimetres; internal angle is radians. Degrees are presentation
only. Coordinates are right-handed. Absolute translation is `PositionB - PositionA`.
An alignment transform always maps B to A and is named `BToA`.

Result reduction is fail-closed. `PASS` requires completed mandatory evidence and a
proven geometry/position match. Probable, ambiguous, incomplete, or failed stages
cannot produce `PASS`.

Imported assembly instances and geometry prototypes are separate domain concepts.
Expensive prototype geometry is fingerprinted, meshed, and deep-compared once; each
instance keeps its own transform.

## Consequences

- GUI and CLI share one comparison service.
- OCCT exceptions and mutable shapes cannot cross adapter boundaries.
- Viewer alignment is explicit presentation state and never mutates source geometry.
- Scheduler tasks receive cancellation, progress, and memory-budget context.
- JSON/CSV reporting consumes the canonical immutable result model.

