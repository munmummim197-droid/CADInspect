# StepCompare DEV V1 architecture

## Dependency direction

```text
StepCompare.exe / stepcompare-cli.exe
                  |
                  v
             Application
                  |
                  v
             Ports + Domain  (standard C++20 only)
                  ^
                  |
 OCCT import/geometry, scheduler, cache, report, diagnostics adapters

 Qt GUI + OCCT viewer consume immutable domain/result snapshots.
```

Planned CMake targets:

```text
stepcompare_domain
stepcompare_ports             -> domain
stepcompare_application       -> ports, domain
stepcompare_occt_import       -> ports, domain, OCCT
stepcompare_occt_geometry     -> ports, domain, OCCT
stepcompare_matching          -> ports, domain
stepcompare_scheduler         -> ports
stepcompare_cache             -> ports, domain
stepcompare_reporting         -> ports, domain
stepcompare_diagnostics       -> ports, domain
stepcompare_viewer            -> domain, Qt6, OCCT visualization
stepcompare_gui               -> application, viewer, Qt6
stepcompare_cli               -> application
```

Public core headers must not contain `QString`, `QObject`, `TopoDS_Shape`, or
`TDocStd_Document`. Composition roots inject concrete adapters into one shared
comparison service.

## Comparison pipeline

```text
hash/config/cache lookup
  -> import A and B
  -> unit normalization and assembly indexes
  -> prototype statistics and invariant fingerprints
  -> tiered component matching
  -> remove proven exact cohorts
  -> absolute placement and rotation
  -> B-to-A alignment hypotheses and independent validation
  -> deep comparison of suspicious prototypes only
  -> bidirectional surface deviation when required
  -> deterministic fail-closed result reduction
  -> report, viewer snapshots, and cache commit
```

Fingerprinting is candidate pruning, not proof. Equal volume or equal bounding box
cannot produce `PASS`.

## Assembly ownership

An `AssemblyNode` is an instance with local/world transforms and a reference to a
`PartPrototype`. Geometry is owned by an import-session `GeometryStore`; domain code
uses an opaque `GeometryId`. Repeated instances share fingerprint, deep result, and
render mesh at prototype level.

OCCT objects are not assumed thread-safe. Mutating operations use a serialized lane
or exclusive per-document/prototype lease. Published domain snapshots are immutable.

## Scheduling and memory

- Bounded `std::jthread` pool with `std::stop_token`.
- Task DAG, cooperative cancellation, structured progress, and deterministic order.
- Weighted memory permits before triangulation, BVH, and Boolean work.
- Under pressure: reduce concurrency, batch prototypes, release intermediates,
  evict LRU cache, and defer high-quality tessellation.
- Never publish a stage result after cancellation.

## Viewer contract

The viewer consumes a `RenderSceneSnapshot` with stable `NodeId` selection and two
orthogonal state axes:

```text
coordinates: Absolute | Aligned
layer:       AOnly | BOnly | Overlay | Difference | Heatmap
```

The active coordinate mode is always visible. Aligned mode uses a presentation-only
transform and never mutates imported geometry. Prototype meshes are shared across
instances; tessellation is progressive and quality is deferred for large models.

## Result semantics

The canonical result carries separate execution, decision, geometry, position,
matching, evidence coverage, diagnostics, timings, cache data, and component results.
`PASS` requires all mandatory evidence to complete and prove the match. Probable,
ambiguous, incomplete, or failed evidence becomes `CHECK` or `ERROR`.

CLI exit codes planned for DEV V1:

```text
0   PASS
1   FAIL / difference
2   CHECK / inconclusive
3   invalid arguments or input
4   STEP import error
5   processing/internal error
130 cancelled
```

