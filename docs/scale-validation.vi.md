# Kiểm chứng quy mô vật lý — DEV V1

## Phạm vi và nguyên tắc

Harness sinh file STEP AP214/XCAF thật bằng OpenCASCADE, sau đó chạy
`dist\stepcompare-cli.exe` Release trong process riêng. Mỗi assembly có một
prototype hộp bất đối xứng và một lưới occurrence đặt bằng transform thật. Hai
file A/B của từng case là bản sao byte-identical.

Phép đo dùng `System.Diagnostics.Stopwatch`, `Process.TotalProcessorTime`, peak
working set và private bytes của Windows; chu kỳ sampling 20 ms. Mỗi report JSON
được đọc lại để xác nhận verdict, completion, exit code và số component row đúng
bằng số occurrence. Harness không xóa Windows file cache và không gắn nhãn lượt
chạy là cold/warm.

Chạy lại bằng:

```powershell
.\scripts\Invoke-ScaleValidation.ps1 `
  -LargeOccurrences 1000 `
  -VeryLargeOccurrences 5000 `
  -Repetitions 3
```

Artifact máy đọc được:

- `build\scale-validation\results\scale-evidence.json`;
- `build\scale-validation\results\scale-summary.csv`;
- `large-run-*.json` và `very-large-run-*.json` trong cùng thư mục.

## Evidence cuối ngày 2026-08-26

- Git HEAD: `f083e85d92e174759ccfda64a4112801908e0fe4`.
- `gitWorktreeDirty=false` tại thời điểm đo.
- Release CLI SHA-256:
  `3a76667e8eb3831398cd83a21797c074d0425e736917c76a24561f6905816773`.
- Evidence JSON SHA-256:
  `2007104f67619b94b8274781af9317d0372f67ceae40e1a5c4658eb1fa23ae11`.
- Máy đo tham chiếu: Windows x64, CPU 32 logical processors. Fingerprint chi tiết
  của máy phát triển không thuộc artifact OSS public.

| Case | Occurrence/file | STEP A | Wall time 3 lượt (ms) | Median (ms) | CPU time 3 lượt (ms) | Peak WS max | Peak private max | Verdict |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| large | 1.000 | 620.796 byte | 497,851 / 431,912 / 427,071 | 431,912 | 406,250 / 390,625 / 406,250 | 24.760.320 | 9.383.936 | 3/3 `PASS` |
| very-large | 5.000 | 3.135.197 byte | 14.743,855 / 13.847,743 / 14.289,664 | 14.289,664 | 14.078,125 / 13.546,875 / 14.062,500 | 43.986.944 | 30.584.832 | 3/3 `PASS` |

Mọi process đều exit `0`, report execution hoàn tất, lý do
`SAME_GEOMETRY_SAME_POSITION`, và lần lượt có đúng 1.000/5.000 component row.
`allCasesPassed=true` và `allCasesCompleted=true`.

SHA-256 fixture cuối:

- large A/B:
  `37ebcbc67fe779c2df9719762a1e3ee557cb8a3c87835a0918566971566f0801`;
- very-large A/B:
  `22f7fe5ef832540739475ab9fcf84e05dbaddd35f52514c2fafe3647820b3447`.

Header STEP có timestamp do OCCT writer tạo nên hash có thể đổi khi sinh lại;
trong từng case, harness luôn tự xác nhận A/B byte-identical.

## Tính trung thực của nhánh exact identity

PASS không dựa riêng vào tên file hoặc hash. Pipeline yêu cầu SHA-256 và kích
thước bằng nhau, vẫn import/index cả A và B, rồi xác nhận occurrence stable ID,
prototype ID và world transform. Chỉ sau proof này mới gán deviation chính xác
max/mean/RMS/p95 bằng 0 với `sampleCount=0` và không gọi surface sampler. Vì vậy
đây là shortcut chứng minh, không phải fake benchmark.

## GUI và cancellation vật lý

Trên `dist\StepCompare.exe` thật:

- A/B very-large 5.000 occurrence được nạp, render và canonical compare trong nền;
  message loop vẫn phản hồi khi import và compare.
- Summary GUI là `PASS — SAME_GEOMETRY_SAME_POSITION`, deviation 0/0/0 mm;
  lượt đầu cache MISS, lượt lặp cache HIT.
- Heatmap được bật và JSON canonical được lưu tại
  `build\scale-validation\results\gui-canonical.json` (SHA-256
  `542812858c40133ea88c995c9e6210a6d959f0ea3e2bc8853ad79a92a0153cea`).
  Report có 5.000 row, `COMPLETED`, all evidence complete, cache hit và deviation
  available.
- Fixture cancellation 20.000 occurrence được import đến progress 5%, nhận Cancel,
  GUI vẫn phản hồi trong lúc chờ checkpoint OCCT, kết thúc `CANCELLED`, và không
  publish kết quả 20.000 occurrence lên scene/cây cũ.

Viewer batch presentation gom toàn bộ occurrence rồi refresh một lần, loại bỏ
đường O(n²) do refresh sau từng shape mà lượt GUI vật lý trước đó đã phát hiện.

## Hạn chế phép đo

- Không có guarantee cold-cache; số liệu chỉ là ba lượt độc lập theo contract đã
  ghi, không gắn nhãn cold/warm.
- Sandbox không cho đọc CIM physical RAM; evidence ghi rõ diagnostic này.
- Aggregate CPU được suy từ process CPU/wall time; chưa có profiler attribution
  theo hàm hoặc theo từng worker.
- Chưa benchmark STEP hàng trăm MB hay > 1 GB. Không suy rộng kết quả 5.000
  occurrence thành cam kết cho quy mô chưa đo.
