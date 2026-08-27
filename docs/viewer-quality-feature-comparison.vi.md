# Viewer Quality & Feature-Level Geometry Comparison

Ngày kiểm chứng cuối: 2026-08-27.

## Đang ở đâu

- Baseline được Owner chấp nhận: `8ae397d4c350d4d2cf1dd47b2b9fc1550a832247`.
- Thay đổi hiện nằm trong working tree của nhánh `main`; chưa commit và không tự
  gắn `DEV_V1_STATUS=DONE`.
- Canonical whole-model verdict, tolerance và các reason hiện hữu không bị đổi.
- Canonical schema được bump có chủ ý lên `1.1`: `1.0` là whole-model/assembly;
  `1.1` thêm top-level feature evidence theo hướng additive, không đổi nghĩa field
  `1.0`. Không tuyên bố unknown-field compatibility cho external consumer chưa
  có trong repository.
- Trạng thái cuối của remediation chỉ được xác lập sau fresh Release, package,
  exact-binary benchmark và GUI vật lý; tài liệu này không thay Owner commit gate.

## Các module viewer đã đổi

- `src/viewer/occt_viewer_widget.cpp` và public viewer headers: presentation,
  camera, clipping plane, selection/feature highlight, colors và render quality.
- `apps/gui/preview_quality.*`: policy Detail/Balanced/Scalable, tessellation
  theo prototype và reuse mesh cho occurrence lặp.
- `apps/gui/step_preview_loader.*` và `step_preview_scene_adapter.*`: đưa policy
  vào pipeline preview, không vi phạm ownership/thread-safety của OCCT.
- `apps/gui/viewer_actions.*` và `main_window.*`: mode Shaded, Shaded + Edges,
  Wireframe, Transparent/X-Ray, Section View cùng A/B/Difference/Heatmap.
- `apps/gui/comparison_results_panel.*` và
  `comparison_readability_model.*`: Feature Comparison Table dùng model/view,
  selection sync và double-click locate/zoom/highlight.
- `include/stepcompare/feature/*`, `src/feature/*` và
  `src/application/feature_evidence.*`: recognition B-Rep và comparison evidence.
- `include/stepcompare/reporting/report.hpp` và reporting writers: trường
  `features` bổ sung trong JSON/CSV canonical report.

## Thay đổi tessellation / presentation

- Bật Phong shading, default lights, satin material, MSAA 4x, depth pre-pass và
  weighted blended OIT; background gradient tối giúp silhouette dễ đọc.
- Shaded + Edges làm rõ seam/outline. A dùng xanh lam, B dùng hổ phách; overlay
  có transparency để không che nhau. Selection dùng vàng, hover dùng cyan.
- Difference dùng palette tương phản. Heatmap vẫn giữ shading/feature và luôn có
  numerical legend liên kết `Max deviation` với surface tolerance.
- Fit All và Fit Selected có margin ổn định; double-click feature dùng bounding
  box của các face evidence thay vì zoom vào toàn model.
- Section View dùng OCCT clipping plane qua tâm vùng nhìn hiện tại, theo hướng
  camera và có capping, cho phép quan sát lỗ bậc, pocket âm và hình học ẩn.
- Policy Scalable chỉ triangulate một lần cho mỗi prototype rồi tái sử dụng cho
  các occurrence; tắt edge overlay ở quy mô rất lớn để giữ responsiveness.

## Feature recognition và comparison

Recognition không dựa vào tên component hay giả định STEP có feature history.
Nó đọc B-Rep/topology và analytic surfaces bằng OCCT:

- Through Hole: cylinder có bằng chứng hai phía xuyên qua vật thể.
- Counterbore/stepped hole: các cylinder đồng trục, khác bán kính và có quan hệ
  bậc được chứng minh.
- Slot: hai đầu trụ/cung cùng bán kính cộng với profile obround; các cylindrical
  split-face thuộc slot được gom lại, không bị báo sai thành Through Hole.
- Blind Pocket và Keyway planar: connected internal planar topology rõ ràng có
  đường `GeometryProven`; recess hở/không đủ topology vẫn trả
  `CHECK / FEATURE_AMBIGUOUS`.
- Fillet: analytic toroidal/cylindrical blend; Chamfer: conical face và góc.

Mỗi feature row chứa type, center, axis/orientation, kích thước chính/phụ, depth,
radius, angle, profile, through/blind, confidence, face indices, owner component,
absolute center và aligned center. Comparison xuất riêng:

- `absoluteDifferenceBMinusAMm = B_absolute - A_absolute`;
- `alignedDifferenceBMinusAMm = B_after_rigid_alignment - A`.

Test dịch nguyên part chứng minh absolute feature delta khác `0 mm`, aligned
delta xấp xỉ `0 mm` và feature vẫn PASS. Test dịch feature cục bộ sau alignment
trả FAIL với `FEATURE_POSITION_CHANGED`. Các reason chính còn có
`FEATURE_SIZE_CHANGED`, `FEATURE_DEPTH_CHANGED`, `FEATURE_RADIUS_CHANGED`,
`FEATURE_ANGLE_CHANGED`, `FEATURE_PROFILE_CHANGED`, `FEATURE_TYPE_CHANGED`,
`FEATURE_NEW`, `FEATURE_MISSING` và `FEATURE_AMBIGUOUS`. Mọi PASS yêu cầu
`GEOMETRY_PROVEN`, alignment được chứng minh và toàn bộ đại lượng trong tolerance;
evidence thiếu hoặc confidence thấp luôn trả CHECK, không suy luận PASS từ tên.

Ma trận regression vật lý sinh và import lại 42 cặp STEP thật: bảy feature nhân
sáu case `SAME_FEATURE_SAME_POSITION`, `FEATURE_MOVED`, `DIMENSION_CHANGED`,
`WHOLE_PART_MOVED`, `FEATURE_LOCAL_MOVED_AFTER_ALIGNMENT` và
`NEGATIVE_OR_AMBIGUOUS`. B không còn là byte-copy duy nhất của A. Ma trận kiểm
tra trực tiếp canonical feature rows qua importer, deep alignment, surface
deviation và recognizer OCCT thật.

## Build result

- Release build hoàn tất.
- Full Release CTest cuối: `20/20` PASS, `0` fail, real time `18,33 s`.
- Package regression hoàn tất; `dist` có 52 tệp, tổng `66.187.584` byte.
- `StepCompare.exe` SHA-256:
  `f91057c45453c6c2c6fbb0ce860ced541d4076427065d71f9bdf10e32f99b451`.
- `stepcompare-cli.exe` SHA-256:
  `e781e4bb55c8223306658f10ae8268d6ce9aa842b8a8b12260891855b95437a3`.

## Test result

- Fixture OCCT thật phủ Through Hole, Blind Pocket, Counterbore, Slot, Fillet,
  Chamfer và planar Keyway/recess ambiguous.
- Regression bắt được và đã sửa false positive: split cylindrical face của slot
  từng bị nhận nhầm là Through Hole. Test hiện bắt buộc slot không sinh hole giả.
- Package CLI trên `hole-slot-pocket-A/B.step`: whole-model PASS; 4 feature row:
  Through Hole PASS, Counterbore PASS, Slot PASS, Blind Pocket CHECK ambiguous.
- Package CLI trên `fillet-chamfer-A/B.step`: whole-model PASS; Fillet và Chamfer
  PASS có evidence hình học; planar recess vẫn CHECK ambiguous.
- Canonical JSON dùng `schemaVersion=1.1`; test dùng Qt JSON parser thật để đọc
  object/array và kiểm tra các field verdict/placement `1.0` giữ nguyên nghĩa.
  Artifact lịch sử trước remediation bên dưới chỉ dùng tham chiếu, không phải
  final candidate mới:
  `build/viewer-quality-results/package-final-hole.json` SHA-256
  `29d66af5c6c8803f94be0af01ed9659457d8469a62af543f4bf701c81992c63d`;
  `package-final-fillet.json` SHA-256
  `fa03e2e883df75ec12038fb1bcd19097619d8926797f397f5e52a6a85e9db527`.
- Physical feature matrix mới chạy 42/42 case STEP thật và in
  `FEATURE_PHYSICAL_MATRIX=PASS`; physical feature-heavy cancellation in
  `FEATURE_CANCELLATION_PHYSICAL=PASS`, không publish kết quả `COMPLETED` cũ sau
  khi cancellation đã được nhận trước publish.
- Package CLI regression trên fixture Keyway moved trả
  `FEATURE_POSITION_CHANGED`; fixture Blind Pocket depth changed trả
  `FEATURE_DEPTH_CHANGED`. Cả hai giữ whole-model FAIL theo canonical reducer
  hiện hữu, không đổi semantics whole-model.

## GUI physical result

Đã chạy trực tiếp package cuối `dist/StepCompare.exe`:

- Fixture hole/slot/pocket thể hiện rõ lỗ xuyên, lỗ bậc, rãnh dài, pocket âm,
  gân và bậc; model không bị nhìn thành tấm phẳng.
- Fixture Keyway positive được import từ STEP thật, nhận
  `Keyway / rãnh then`, `BLIND · RECTANGULAR_KEYWAY` và
  `PASS — EVIDENCE HÌNH HỌC`; double-click feature đã locate, zoom và highlight
  đúng các face keyway, đồng thời đồng bộ Feature Table với tree và viewer.
- Fixture Blind Pocket positive được import từ STEP thật, nhận
  `Blind Pocket / pocket âm`, `BLIND · RECTANGULAR_BLIND_POCKET` và
  `PASS — EVIDENCE HÌNH HỌC`; pocket âm nhìn rõ trong preview vật lý.
- Fixture fillet/chamfer thể hiện cạnh bo, bề mặt cong và lỗ côn/vát rõ trong
  Shaded + Edges. X-Ray làm lộ bore ẩn; Section View cắt qua cone và bore.
- Đã chuyển vật lý qua Wireframe, Transparent/X-Ray và Section View; không crash
  hay mất responsiveness.
- Feature table hiển thị đúng bảy cột, ABS và ALIGNED delta, tolerance, PASS có
  chữ `EVIDENCE HÌNH HỌC`, CHECK có chữ `FEATURE_AMBIGUOUS`; Fillet/Chamfer hiển
  thị through/blind là `N/A`.
- Double-click Fillet đã chọn đúng row, đồng bộ owner part trong tree, fit vào
  face bo và highlight vàng trong viewer. Status xác nhận locate/zoom/highlight.
- Heatmap bật theo thao tác người dùng, đổi presentation và numerical legend vẫn
  ghi rõ max deviation/tolerance; không tự bật ngoài ý muốn.
- A Only hiển thị xanh, B Only hổ phách, Overlay giữ hai presentation phân biệt,
  Difference của cặp đồng nhất không hiện vùng giả; Aligned vẫn chuyển được.

## Performance impact

Physical CLI benchmark mới nhất trong vòng này:

| Case | Occurrence/file | Wall | CPU | Peak working set | Peak private | Kết quả |
|---|---:|---:|---:|---:|---:|---|
| large | 1.000 | 491,325 ms / 0,491325 s | 375,000 ms | 24.846.336 byte / 23,695 MiB | 9.170.944 byte | PASS, 1.000 row |
| very-large | 5.000 | 8.770,270 ms / 8,770270 s | 8.250,000 ms | 41.648.128 byte / 39,719 MiB | 27.148.288 byte | PASS, 5.000 row |

Evidence: `build/scale-validation/results/scale-evidence.json`, SHA-256
`8229f8ccf3d4cabe10f7febbfd234c4058bba0575f3be5d033a692504cef47e3`.
`releaseCli.sha256` trong evidence là
`e781e4bb55c8223306658f10ae8268d6ce9aa842b8a8b12260891855b95437a3`,
trùng tuyệt đối SHA-256 của `dist/stepcompare-cli.exe` (`MATCH=True`).
Harness ghi rõ không giả lập benchmark, không bảo đảm cold cache và CLI không tự
chứng minh GUI responsiveness.

GUI package cuối đã nạp/compare A và B của fixture 5.000 occurrence, hiển thị
`PASS — GIỐNG NHAU` và `5000 / 5000` trong bảng model/view. Đổi filter sang
`Moved` trả `0 / 5000` tức thời; A Only, B Only, Overlay, Difference và Aligned
đều chuyển được sau khi đã nạp 5.000 occurrence. Viewer dùng một mesh prototype
được reuse cho 4.999 occurrence còn lại.

## Known limitations

- STEP không có feature history chuẩn hóa. Planar keyway/blind pocket không đủ
  topology vẫn là CHECK; đây là fail-closed có chủ ý. External consumer không có
  trong repository nên unknown-field compatibility của consumer đó không thể
  chứng minh; schema `1.1` tránh claim sai.
- Recognition Fillet/Chamfer hiện mạnh nhất với analytic torus/cylinder/cone;
  spline approximation có thể chưa được nhận hoặc phải CHECK.
- Section plane hiện tự đặt qua tâm view và theo camera; chưa có gizmo/slider để
  Owner kéo offset mặt cắt.
- `CURRENT_HEATMAP_TYPE=COMPONENT_LEVEL_DEVIATION`; chưa phải per-face,
  per-vertex, per-point hay surface scalar texture trên từng tam giác.
- Benchmark không đo model hàng trăm MB/>1 GB và không được suy rộng tới quy mô
  chưa kiểm thử.
- Packaging ghi warning `windeployqt` không query được `qtpaths`; fallback plugin
  tối thiểu đã chạy và package validation/GUI smoke đều PASS.

## Owner review readiness

Vòng refinement này sẵn sàng để Owner review: có code, test, fixture STEP thật,
canonical JSON, package Release và GUI vật lý trên cả feature-rich parts lẫn
assembly 5.000 occurrence. Chưa có commit/checkpoint mới và không tuyên bố
`DEV_V1_STATUS=DONE` thay Owner.
