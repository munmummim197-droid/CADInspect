# Kiểm chứng quy mô vật lý — DEV V1

## Phạm vi và nguyên tắc

Harness này sinh file STEP AP214/XCAF thật bằng OpenCASCADE, sau đó chạy
`dist\stepcompare-cli.exe` Release trong process riêng. Mỗi assembly có một
prototype hộp bất đối xứng và một lưới occurrence được đặt bằng transform thật.
Hai file A/B của từng case là bản sao byte-identical.

Phép đo dùng `System.Diagnostics.Stopwatch`, `Process.TotalProcessorTime`, peak
working set và private bytes của Windows. Sampling process là 20 ms. Mỗi report
JSON được đọc lại để xác nhận số component row đúng bằng số occurrence yêu cầu.
Harness không xóa Windows file cache và không gắn nhãn lần chạy là cold/warm.

Chạy lại bằng:

```powershell
.\scripts\Invoke-ScaleValidation.ps1 `
  -LargeOccurrences 1000 `
  -VeryLargeOccurrences 5000 `
  -Repetitions 3
```

Evidence máy đọc được nằm trong thư mục build bị Git ignore:

- `build\scale-validation\results\scale-evidence.json`
- `build\scale-validation\results\scale-summary.csv`
- report canonical của từng lượt chạy trong cùng thư mục.

## Bằng chứng vật lý ngày 2026-08-26

Đợt đo dưới đây dùng Release CLI SHA-256
`3fc6afeb3f7296445a0b44a8836a04561d35b24af5c2766ccdeae0758d23dce3`.
Binary này là package trước completion round; vì vậy kết quả là baseline vật lý,
không phải bằng chứng nghiệm thu package cuối.

| Case | Occurrence/file | Kích thước STEP A | Wall time 3 lượt (ms) | Median (ms) | Peak working set lớn nhất | Component row | Verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| large | 1.000 | 620.796 byte | 517,277 / 477,867 / 490,148 | 490,148 | 25.395.200 byte | 1.000 | `CHECK EVIDENCE_INCOMPLETE` |
| very-large | 5.000 | 3.135.197 byte | 8.820,049 / 8.423,573 / 8.465,101 | 8.465,101 | 41.922.560 byte | 5.000 | `CHECK EVIDENCE_INCOMPLETE` |

Các process đều hoàn tất bình thường với exit code `2` (CHECK), report tồn tại,
và số component row đúng bằng số occurrence. Đây không phải PASS. Với hai input
byte-identical, matcher hiện chỉ chứng minh geometry nhưng để placement của các
instance lặp lại ở `UNKNOWN`; reducer do đó trả CHECK đúng fail-closed.

Hiệu năng quan sát cho thấy 5 lần occurrence làm wall time tăng khoảng 17 lần,
trong khi aggregate process CPU xấp xỉ một logical core. Đây là dấu hiệu cần
được đánh giá lại sau khi hoàn tất matching/cache; tài liệu này không suy diễn
nguyên nhân thành kết luận profiler.

SHA-256 của fixture ở lượt baseline này:

- large A/B: `b8e71837781b36e5925710c2022b6a5330746a652501d459c606e5dbcf5a8c0e`;
- very-large A/B: `6328b93dd90f31894085c74e5ef3ef583b5a7763539f278261d94c212738ab55`.

Header STEP có timestamp do OCCT writer tạo, nên hash có thể thay đổi khi sinh
lại; trong từng case, harness luôn kiểm tra A/B byte-identical.

## Những gì chưa được chứng minh

- Chưa đo message-loop responsiveness của GUI khi tải chính hai fixture này.
- Chưa đo cooperative cancellation của coordinator/GUI; kill process không được
  coi là bằng chứng cancellation.
- Chưa có cold-cache guarantee hay profiler attribution.
- Chưa có benchmark lại trên Release package cuối của completion round.

Vì các khoảng trống trên và verdict CHECK của baseline, tài liệu này không đủ để
tự tuyên bố `DEV_V1_STATUS=DONE`.
