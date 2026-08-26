# StepCompare DEV V1

StepCompare là ứng dụng Windows native mới để so sánh chi tiết và cụm lắp ráp
STEP/STP. Lõi so sánh C++20 độc lập với Qt và được dùng chung bởi
`StepCompare.exe` và `stepcompare-cli.exe`.

Thiết kế tuân theo nguyên tắc fail-closed: dữ liệu gần giống, bằng chứng thiếu,
không đối xứng rõ ràng hoặc phép tính sâu không hoàn tất sẽ không bao giờ được
báo `PASS`; kết quả tương ứng là `CHECK` hoặc `ERROR`.

## Trạng thái hiện tại

Phần nền tảng, import STEP/XCAF, so khớp assembly, kiểm tra hình học sâu,
surface-deviation BVH, scheduler, cache, JSON/CSV, CLI, Qt/OCCT viewer và quy
trình đóng gói Release đã được triển khai và có kiểm thử tự động.

`DEV_V1_STATUS=NOT_DONE`

Các cổng nghiệm thu còn thiếu gồm benchmark với fixture STEP lớn/rất lớn, đo
RAM/CPU/thời gian, nối cache và surface-deviation vào toàn bộ coordinator/report,
và heatmap. Chi tiết và bằng chứng xem tại
[`docs/dev-v1-status.vi.md`](docs/dev-v1-status.vi.md).

## Baseline phụ thuộc

- vcpkg builtin registry: `f89a4a1da4e3176a8d1a14c1825b9b2f98e48843`
- Qt: `6.11.1#1`
- OpenCASCADE: `8.0.1`
- Triplet: `x64-windows`
- Toolchain đã kiểm chứng: MSVC v145 x64, Windows SDK 10.0.26100.0

## Build và test

Môi trường Codex trên host có hai biến `Path`/`PATH` khác kiểu chữ khiến MSBuild
từ chối chạy. Wrapper dưới đây chỉ chuẩn hóa môi trường của tiến trình con, không
thay đổi cấu hình máy:

```powershell
.\scripts\Invoke-StepCompareBuild.ps1 -Preset core-dev -Stage All
.\scripts\Invoke-StepCompareBuild.ps1 -Preset full-dev -Stage All
.\scripts\Invoke-StepCompareBuild.ps1 -Preset full-dev -Stage All -Configuration Release
```

Đóng gói portable Release:

```powershell
.\scripts\New-StepComparePortablePackage.ps1 -FreshBuild
```

Kết quả nằm trong `dist\` và gồm GUI, CLI, Qt/OCCT DLL, Qt platform/image/style
plugins và MSVC CRT cần thiết.

## CLI

```powershell
.\dist\stepcompare-cli.exe <file-a.step> <file-b.step> --deep `
  --json report.json --csv report.csv
```

Mã thoát: `0=PASS`, `1=FAIL`, `2=CHECK`, `3=đối số/input không hợp lệ`,
`4=lỗi import`, `5=lỗi xử lý nội bộ`, `130=đã hủy`.

Ngưỡng mặc định là `0.01 mm` cho vị trí/bề mặt và `0.01°` cho góc. Vector dịch
chuyển tuyệt đối là `Delta = File B - File A`; mọi transform căn chỉnh đều mang
nghĩa `B -> A`.
