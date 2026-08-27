# Báo cáo trạng thái StepCompare DEV V1

Ngày chốt bằng chứng: 2026-08-26.

## Kết luận

`DEV_V1_STATUS=DONE`

Kết luận áp dụng cho Definition of Done DEV V1 đã chốt. Nguồn được nghiệm thu tại
commit `f083e85d92e174759ccfda64a4112801908e0fe4`; worktree sạch khi chạy bộ
benchmark cuối. Không mở thêm feature ngoài phạm vi DEV V1.

## Bằng chứng chức năng

- Canonical comparison là nguồn kết quả duy nhất cho CLI, GUI, JSON và CSV.
- Cache thật đã nối vào pipeline; GUI vật lý xác nhận lượt đầu `MISS`, lượt lặp
  cùng input/config là `HIT`.
- Surface deviation hai chiều dùng triangulation + BVH và xuất
  max/mean/RMS/p95, sample count và triangle-distance evaluation count.
- Heatmap/deviation coloring là fail-closed: mapping stable ID thiếu, trùng hoặc
  không hợp lệ bị từ chối nguyên khối; input mới làm mất hiệu lực màu cũ.
- Alignment đối xứng chỉ được PASS sau Boolean overlap proof. Candidate từ
  principal axes được sinh quyết định; trường hợp chưa chứng minh được vẫn trả
  `CHECK`, không false PASS.
- Exact byte identity chỉ đi đường zero-deviation sau SHA-256 + kích thước bằng
  nhau, cả hai file vẫn được OCCT import/index và hierarchy, prototype, stable ID,
  world transform đều được kiểm chứng. `sampleCount=0` ở nhánh này là chứng minh
  chính xác, không phải phép đo bề mặt giả.
- Scheduler dùng hàng đợi/worker giới hạn và memory budget. OCCT dùng object bất
  biến hoặc task-local và được serialize tại các đường không an toàn.
- JSON/CSV UTF-8, số bất biến theo locale, từ chối non-finite; report mang input
  identity, version, tolerance, execution/cache metadata, statistics, placement,
  deep deviation, timing, verdict và component rows.

## Kiểm thử tự động và Release

- Final Release regression: `16/16` test đạt, `0` test lỗi.
- Test phủ domain/reducer, assembly, scheduler/cancel, reporting, cache,
  coordinator/application, CLI, STEP import Unicode, deep geometry, surface
  deviation, dependency contract và viewer state/coloring/selection/preview.
- Các fixture bắt buộc gồm exact copy, translate XYZ, rotate, đổi kích thước,
  thêm/bỏ lỗ, cylinder/symmetry, open shell, moved/modified/missing/new component
  và đường dẫn Unicode thật.
- CLI/package Release được chạy bằng file STEP AP214/XCAF vật lý, không dùng số
  liệu mô phỏng.

## Benchmark assembly vật lý

Harness sinh assembly AP214/XCAF thật rồi chạy mỗi case ba process Release độc
lập. Evidence cuối gắn với commit sạch `f083e85`:

| Case | Occurrence/file | Wall time 3 lượt (ms) | Peak working set lớn nhất | Kết quả |
|---|---:|---:|---:|---|
| large | 1.000 | 497,851 / 431,912 / 427,071 | 24.760.320 byte | 3/3 `PASS`, exit 0, đúng 1.000 row |
| very-large | 5.000 | 14.743,855 / 13.847,743 / 14.289,664 | 43.986.944 byte | 3/3 `PASS`, exit 0, đúng 5.000 row |

Cả sáu report đều là `SAME_GEOMETRY_SAME_POSITION`; A/B trong từng case được
harness xác nhận byte-identical. Chi tiết CPU, private bytes, hash fixture/report
và hạn chế phép đo nằm trong `docs/scale-validation.vi.md` và artifact
`build/scale-validation/results/scale-evidence.json`.

## Kiểm thử GUI vật lý

Đã chạy trực tiếp `dist\StepCompare.exe` trong interactive Windows session:

- Nạp cả A và B của fixture 5.000 occurrence qua hộp thoại Windows; GUI vẫn đáp
  ứng trong lúc import/compare và render đủ cây cùng presentation theo occurrence.
- Canonical summary hiển thị
  `PASS — SAME_GEOMETRY_SAME_POSITION | deviation max/mean/RMS: 0 / 0 / 0 mm`.
- So sánh lặp xác nhận `cache: HIT`; heatmap bật thành công; Save JSON tạo report
  5.000 component với execution `COMPLETED`, evidence complete và cache hit.
- A only, B only, Overlay, Difference, Absolute/Aligned, camera chuẩn và Fit All
  đã được regression trên viewer thật. Pan/zoom/rotate và đồng bộ tree/viewer có
  coverage trong các test viewer state/selection presenter.
- Kiểm thử hủy dùng STEP vật lý 20.000 occurrence. GUI nhận Cancel ở progress 5%,
  chuyển sang `Cancelling after current OCCT checkpoint`, vẫn phản hồi, rồi kết
  thúc `File A — CANCELLED — Cancelled`. Kết quả mới không được publish; scene/cây
  5.000 occurrence trước đó được giữ nguyên.

Cancellation là boundary semantics trung thực: không tuyên bố ngắt được ở giữa
một lời gọi OCCT không cooperative; yêu cầu hủy được kiểm tra tại checkpoint và
mọi kết quả trả về sau hủy bị chặn publish.

## Portable package cuối

- Package root: `dist/` (tương đối từ repository root).
- 52 tệp, tổng `65.933.632` byte.
- `StepCompare.exe` SHA-256:
  `27d6ff90be491e1c8d2ae43b41085cd9241776b276bed4b0aaeec2c573687dff`.
- `stepcompare-cli.exe` SHA-256:
  `3a76667e8eb3831398cd83a21797c074d0425e736917c76a24561f6905816773`.
- Có Qt platform/image/style plugins, Qt/OCCT DLL và MSVC CRT. Package chạy theo
  mô hình copy-folder rồi mở `StepCompare.exe`, không cần compiler.

## Ranh giới không được suy diễn

- Benchmark này chứng minh 1.000 và 5.000 occurrence; fixture 20.000 chỉ dùng cho
  cancellation. Chưa có phép đo STEP > 1 GB, nên không coi đó là năng lực đã
  chứng minh vật lý.
- Harness không ép cold cache và không thực hiện profiler attribution.
- Trường hợp alignment/deep evidence không đủ vẫn phải trả `CHECK`/`ERROR`; trạng
  thái DEV V1 DONE không nới lỏng chính sách fail-closed.
