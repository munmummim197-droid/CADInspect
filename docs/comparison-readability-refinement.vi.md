# UI/UX Comparison Readability Refinement

Ngày kiểm chứng: 2026-08-26.

## Kết luận

`COMPARISON_READABILITY_REFINEMENT_STATUS=DONE`

Vòng refinement bắt đầu từ DEV V1 accepted baseline
`8ae397d4c350d4d2cf1dd47b2b9fc1550a832247`. Canonical comparison report vẫn là
nguồn dữ liệu duy nhất cho GUI; không thay đổi comparison semantics, tolerance
hay quy tắc verdict. Một defect UI thực tế được sửa: nạp deviation colors từng
vô tình bật heatmap dù action trên toolbar đang tắt. Trạng thái heatmap nay chỉ
bật khi người dùng yêu cầu và có evidence hợp lệ.

## Nội dung đã triển khai

- Summary tổng thể bằng tiếng Việt, có biểu tượng và chữ rõ ràng cho:
  `GIỐNG NHAU`, `CÙNG HÌNH HỌC / KHÁC VỊ TRÍ`, `HÌNH HỌC THAY ĐỔI`,
  `CHECK / MƠ HỒ`, và `ERROR`.
- Bảng `Comparison Parameters` dùng `QAbstractTableModel`/`QTableView`, có đúng
  sáu cột: `Thông số`, `File A`, `File B`, `Sai lệch B - A`, `Dung sai`,
  `Kết quả`.
- Dữ liệu được chia thành sáu nhóm: kích thước, vị trí/tọa độ, góc xoay, hình
  học, sai lệch bề mặt và thông tin file. Bảng hiển thị Size X/Y/Z, delta
  X/Y/Z, center of mass, rotation angle, Rx/Ry/Rz, volume, surface area, số
  solid/shell/face/edge/vertex, max/mean/RMS/p95 deviation, tolerance và kết
  quả bằng chữ `PASS`/`FAIL`/`CHECK`.
- Mọi sai lệch có dấu tuân thủ duy nhất quy ước `File B - File A`.
- Bảng assembly riêng có bảy cột: `Component`, `Status`, `ΔX`, `ΔY`, `ΔZ`,
  `Rotation`, `Max deviation`; có đủ tám filter yêu cầu.
- Chọn một dòng đồng bộ bảng kết quả, assembly tree và 3D viewer. Double-click
  thực hiện locate, zoom và highlight.
- Heatmap có numerical legend liên kết max deviation và surface tolerance; trạng
  thái vẫn được diễn đạt bằng chữ, không phụ thuộc chỉ vào màu.
- Số được canh phải, dùng locale tiếng Việt, dấu phân cách hàng nghìn, precision
  ổn định và đơn vị rõ; tránh scientific notation không cần thiết.
- Cả hai bảng dùng model/view và proxy filter, không tạo widget riêng cho từng
  dòng. Test model trực tiếp xác nhận 5.000 dòng.

## Regression tự động và package Release

- Full Release tests: `17/17` đạt, `0` lỗi.
- Test mới phủ năm presentation verdict, sáu cột/sáu nhóm thông số, quy ước
  `B - A`, formatting, canh số, tám filter, stable ID hai phía và model 5.000
  component.
- Canonical JSON regression đạt; artifact vật lý:
  `build/scale-validation/results/gui-readability-refinement.json`.
- Package portable: 52 tệp, tổng 66.006.848 byte.
- `StepCompare.exe` SHA-256:
  `b9b2064e16684da81edad52aca193a11bb65ff642b3ef72358371afdd68a7f0c`.
- `stepcompare-cli.exe` SHA-256:
  `3a76667e8eb3831398cd83a21797c074d0425e736917c76a24561f6905816773`.
- `gui-readability-refinement.json` SHA-256:
  `04e28a2b18998dba51c7b2fbddcd5c2e3b81e1541488c30464d2112e97655d2a`.

## Kiểm thử GUI vật lý với 5.000 occurrence

Đã chạy trực tiếp `dist/StepCompare.exe`, mở fixture AP214/XCAF vật lý A/B
5.000 occurrence và kiểm tra trên cửa sổ GUI thật:

- GUI vẫn phản hồi trong lúc nạp và so sánh; summary cuối hiển thị
  `PASS — GIỐNG NHAU`, lượt đầu `Cache: MISS`, lượt lặp `Cache: HIT`.
- Bảng thông số hiển thị vật lý đủ sáu cột và sáu nhóm. Các vùng được cuộn và
  quan sát trực tiếp gồm Size X/Y/Z, center of mass, delta X/Y/Z, rotation và
  Rx/Ry/Rz, volume/surface area/topology counts, max/mean/RMS/p95 deviation,
  tolerance, đường dẫn, SHA-256 và kích thước file.
- Formatting quan sát được gồm `4.930,000 mm³`, `1.906,000 mm²`,
  `0,0100 mm` và `3.135.197 byte`; kết quả `PASS` xuất hiện bằng chữ.
- Tab assembly hiển thị `5000 / 5000` dòng bằng model/view. Danh sách filter có
  đủ tám mục; chọn `Moved` trên cặp file đồng nhất cho kết quả `0 / 5000`, sau
  đó trả về `Tất cả` cho `5000 / 5000`.
- Single-click một component làm đồng bộ dòng bảng và assembly tree, đồng thời
  highlight viewer. Double-click làm locate, zoom và highlight rõ trên 3D.
- Heatmap ban đầu thật sự tắt và legend ghi `Heatmap TẮT`; sau khi bật toolbar,
  viewer đổi màu và legend ghi `Heatmap BẬT — Max deviation: 0,0000 mm |
  Dung sai bề mặt: 0,0100 mm`.
- JSON lưu từ GUI có verdict `PASS`, execution `COMPLETED`,
  `allRequiredEvidenceComplete=true`, đúng 5.000 component và
  max/mean/RMS deviation đều bằng 0.

Fixture vật lý này là cặp đồng nhất, nên chỉ chứng minh trực tiếp dòng component
`PASS — KHÔNG ĐỔI` và hành vi filter rỗng cho `Moved`. Các trạng thái moved,
rotated, geometry changed, missing, new và ambiguous được kiểm tra tự động bằng
report fixture có kiểm soát; không tuyên bố đã quan sát vật lý các trạng thái đó
trên bộ 5.000 occurrence.
