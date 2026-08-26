# Báo cáo trạng thái StepCompare DEV V1

Ngày chốt bằng chứng: 2026-08-26.

## Kết luận

`DEV_V1_STATUS=NOT_DONE`

Bản hiện tại là một foundation chạy được từ đầu đến cuối trên Windows native,
nhưng chưa thỏa toàn bộ Definition of Done của master prompt. Kết luận này là
fail-closed và không biến các hạng mục chưa đo hoặc chưa nối dây thành bằng chứng.

## Phạm vi đã triển khai

- Domain/reducer fail-closed; fast invariants; placement `B - A`; quaternion,
  rotation và nhận diện mơ hồ do đối xứng.
- Scheduler `std::jthread` có queue giới hạn, `stop_token`, progress và điều chỉnh
  số worker theo áp lực bộ nhớ.
- Cache LRU giới hạn bộ nhớ với key phiên bản hóa gồm SHA-256 thật, kích thước,
  mtime, cấu hình import và phiên bản thuật toán.
- JSON/CSV UTF-8, RFC 4180, bất biến theo locale và từ chối số không hữu hạn.
- Import STEP/XCAF qua OCCT: đường dẫn Unicode, hierarchy, prototype/instance,
  local/world transform, chuẩn hóa đơn vị mm và diagnostic fail-closed.
- Assembly index, tiered component matching và khử lặp phép so sâu theo prototype.
- Deep geometry OCCT: alignment cho solid bất đối xứng được hỗ trợ, Boolean
  Common/Vdiff, thay đổi kích thước/lỗ và fail-closed với shell hở/đối xứng.
- Surface deviation hai chiều bằng triangulation và BVH nearest-triangle, có
  max/mean/RMS/percentile và cancellation.
- Qt/OCCT GUI: nạp A/B nền, tiến độ/hủy, cây linh kiện, A only/B only/Overlay/
  Difference, Absolute/Aligned, camera chuẩn và Fit All.
- CLI dùng chung coordinator; xuất JSON/CSV; đóng gói Release portable.

## Bằng chứng tự động

- Core fresh build: `7/7` test đạt.
- Full Debug fresh build: `15/15` test đạt.
- Full Release build/package: `15/15` test đạt.
- CLI trên fixture STEP thật có đường dẫn Unicode, A bằng B và `--deep`:
  `PASS SAME_GEOMETRY_SAME_POSITION`, exit code `0`.
- SHA-256 fixture:
  `e28fc77a6f9d33b8c77cd6b31b013e552ea3d8e227407511dc4c3d4c4546e168`.
- CLI được chạy lại trực tiếp từ `dist\stepcompare-cli.exe` với cùng fixture và
  tiếp tục trả `PASS`, exit code `0`.

## Bằng chứng đóng gói

- Package root: `D:\DW\StepCompare\dist`.
- 52 tệp, tổng `65,469,248` byte.
- `StepCompare.exe` SHA-256:
  `b86519c39cb676fcfbd4d744347ef7caa974d5256d4d10eb56f0ace5fcbcc641`.
- `stepcompare-cli.exe` SHA-256:
  `3fc6afeb3f7296445a0b44a8836a04561d35b24af5c2766ccdeae0758d23dce3`.
- Có `platforms\qwindows.dll`, image-format/style plugins, Qt/OCCT DLL và MSVC CRT.
- `windeployqt` không spawn được `qtpaths` trong sandbox; script dùng fallback sao
  chép plugin tối thiểu đã pin và kiểm tra các tệp bắt buộc.

## Kiểm thử GUI vật lý

Đã mở `dist\StepCompare.exe` trong desktop session thật và nạp cùng fixture
Unicode vào cả A lẫn B qua hộp thoại Windows:

- Hai lượt đều báo `COMPLETED — Preview ready`, progress `100%`.
- Cây linh kiện hiển thị `Chi tiết 01.step` và translator OCCT cho cả A/B.
- A Only hiển thị màu A; B Only hiển thị màu B; Difference rỗng với hai file
  giống nhau; Overlay hoạt động.
- Chuyển Aligned đổi banner thành `ALIGNED GEOMETRY (B -> A)`; nút Isometric
  nhận thao tác.
- Lượt kiểm thử đầu phát hiện vùng OCCT chỉ chiếm một ô nhỏ. Adapter đã được đổi
  sang `Aspect_NeutralWindow`, nhận kích thước trực tiếp từ Qt trong show/resize
  event, sau đó build/package lại. Lượt kiểm thử cuối xác nhận viewport lấp đầy
  widget, trihedron đúng góc và mô hình A/B được Fit All trên toàn vùng.

## Khoảng trống bắt buộc còn lại

- Chưa có fixture/benchmark assembly lớn và rất lớn; chưa đo wall time, peak RAM,
  mức dùng CPU, throughput và độ phản hồi UI theo tiêu chí master prompt.
- Surface-deviation engine đã có test riêng nhưng chưa được nối đầy đủ vào
  coordinator và canonical report.
- Cache đã có test riêng nhưng chưa được dùng trọn vẹn trong pipeline thực.
- Chưa có heatmap/deviation coloring trong viewer.
- GUI đang preview A/B và cây linh kiện nhưng chưa trình bày toàn bộ canonical
  comparison result như CLI.
- Cancellation không thể ngắt giữa một lời gọi OCCT không hợp tác; chỉ có thể
  chặn việc publish kết quả sau khi hủy.
- Alignment hiện giới hạn ở các solid có principal inertia đủ bất đối xứng; trường
  hợp đối xứng trả `CHECK` đúng chính sách fail-closed.

## Môi trường và ràng buộc

Project nằm tại `D:\DW\StepCompare` thay vì `D:\StepCompare` vì sandbox chỉ cho
phép ghi trong workspace `D:\DW`. Git đã khởi tạo trên nhánh `main`.

VM Hyper-V có tồn tại nhưng đang ở trạng thái Saved và được dành cho bộ replay
khác; không sử dụng. Host đã được kiểm tra phù hợp để chạy binary C++ chưa ký;
không thay đổi Windows Security, Defender, AppLocker, WDAC hay chính sách máy.
