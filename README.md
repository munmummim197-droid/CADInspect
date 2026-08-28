# CADInspect

CADInspect là ứng dụng Windows native dùng để so sánh hình học và cấu trúc của
hai tệp STEP/STP. Lõi C++20 được dùng chung bởi giao diện Qt/OpenCASCADE và công
cụ dòng lệnh.

Kết quả được tổng hợp theo nguyên tắc fail-closed: khi bằng chứng hình học thiếu,
mơ hồ hoặc một phép kiểm tra bắt buộc chưa hoàn tất, CADInspect trả `CHECK` hoặc
`ERROR`, không suy luận `PASS`.

## Tổng quan

CADInspect hỗ trợ so sánh part và assembly, phân biệt thay đổi vị trí với thay
đổi hình học sau rigid alignment, và xuất bằng chứng máy đọc được. Quy ước tọa
độ là `Difference = File B - File A`; transform căn chỉnh có nghĩa `B -> A`.

## Tính năng

- Mở STEP/STP dạng part hoặc assembly bằng OpenCASCADE/XCAF.
- Kết quả tổng thể `PASS`, `FAIL`, `CHECK`, `ERROR` và lý do canonical.
- So khớp occurrence có xét assembly path, transform và bằng chứng hình học.
- So sánh vị trí, rotation, volume, surface area và topology counts.
- Surface deviation Max/Mean/RMS, Difference và Heatmap có bằng chứng số.
- Feature-level evidence cho các feature hình học nhận diện được; trường hợp
  không đủ confidence được đánh dấu `CHECK`/ambiguous.
- Viewer CAD với Shaded + Edges, Wireframe, X-Ray, Overlay, Difference,
  Heatmap, Section View và isolate part pair.
- Đồng bộ selection giữa assembly tree, bảng Part/Feature và 3D viewer.
- Xuất báo cáo canonical JSON schema 1.1 và CSV an toàn khi mở bằng spreadsheet.
- GUI `CADInspect.exe` và CLI `stepcompare-cli.exe`.

STEP thường không chứa feature history gốc. Feature recognition của CADInspect
dựa trên B-Rep/surface/topology và không cam kết khôi phục đầy đủ design intent.

## Yêu cầu

- Windows 10/11 x64.
- Visual Studio 2022 trở lên, workload **Desktop development with C++**.
- CMake 3.28 trở lên.
- Git và vcpkg; đặt biến môi trường `VCPKG_ROOT` trỏ tới checkout vcpkg.
- Khoảng trống đĩa phù hợp để build Qt 6 và OpenCASCADE.

Dependency manifest hiện ghim vcpkg baseline
`f89a4a1da4e3176a8d1a14c1825b9b2f98e48843` với Qt và OpenCASCADE theo dynamic
triplet `x64-windows`.

## Build từ source

```powershell
git clone <CADInspect-repository-url>
cd CADInspect
$env:VCPKG_ROOT = 'C:\path\to\vcpkg'
cmake --preset oss-release
cmake --build --preset oss-release
ctest --preset oss-release
```

Preset `oss-release` bật GUI, CLI, OpenCASCADE và toàn bộ test ở cấu hình Release.
Các preset `core-dev`/`full-dev` là preset phát triển cục bộ đã được giữ lại.

Trên môi trường phát triển hiện tại có thể dùng wrapper:

```powershell
.\scripts\Invoke-StepCompareBuild.ps1 -Preset full-dev -Stage All -Configuration Release
```

Tạo package portable cục bộ:

```powershell
.\scripts\New-StepComparePortablePackage.ps1 -FreshBuild
```

Output sinh ra trong `dist\`, có manifest SHA-256 và thông báo license; output
này bị loại khỏi Git.

## Sử dụng

Mở GUI, chọn hoặc kéo-thả hai tệp STEP/STP vào A và B, sau đó bấm Compare. Có thể
mở cửa sổ so sánh mới để giữ nguyên phiên hiện tại.

CLI:

```powershell
.\dist\stepcompare-cli.exe A.step B.step --deep `
  --json report.json --csv report.csv
```

Mã thoát: `0=PASS`, `1=FAIL`, `2=CHECK`, `3=input/argument không hợp lệ`,
`4=lỗi import`, `5=lỗi xử lý`, `130=đã hủy`.

Ngưỡng mặc định là `0.01 mm` cho vị trí/bề mặt và `0.01°` cho góc.

## Cấu trúc project

- `apps/gui/`: ứng dụng desktop Qt và orchestration UI.
- `apps/cli/`: giao diện dòng lệnh.
- `include/stepcompare/`, `src/`: domain, application và OCCT adapters.
- `tests/`: test tự động theo module.
- `benchmark/`: generator/harness fixture tổng hợp.
- `resources/`: icon và Windows/Qt resources do project sở hữu.
- `scripts/`: build, benchmark và portable packaging.
- `installer/`: cấu hình Inno Setup; không chứa installer đã build.
- `docs/`: kiến trúc, evidence và hướng dẫn release.

## Bảo mật và quyền riêng tư

Hãy coi mọi STEP/STP là dữ liệu không tin cậy. File được parse bằng thư viện CAD
native trong tiến trình và model rất lớn/crafted có thể tiêu tốn đáng kể CPU/RAM.
Không chạy CADInspect với quyền cao hơn mức cần thiết. Xem
[`SECURITY.md`](SECURITY.md) để báo lỗ hổng riêng tư.

Báo cáo canonical chứa đường dẫn đầy đủ, SHA-256 và metadata của input nhằm phục
vụ truy vết. Hãy kiểm tra hoặc redaction trước khi chia sẻ report ra ngoài tổ chức.

Xem [Privacy policy](docs/PRIVACY.md) để biết phạm vi xử lý dữ liệu cục bộ và
hành vi network đã được audit.

## Code signing policy

Code signing thông qua SignPath Foundation đang được lên kế hoạch và chờ phê
duyệt. Binary hiện tại không được tuyên bố là đã ký bởi SignPath. Xem
[Code signing policy](docs/CODE_SIGNING_POLICY.md) để biết ranh giới binary do
dự án sở hữu, vai trò phê duyệt và quy trình kiểm chứng dự kiến.

## Releases

Binary, PDB, archive và installer không được commit. Quy trình phát hành dự kiến
là clean Release build, test, package, kiểm tra manifest và xác minh trạng thái
ký trước khi công bố. Authenticode signing chỉ được thực hiện sau khi nhà cung
cấp phê duyệt; initial unsigned release phải được ghi nhãn rõ. Xem
[`docs/releasing.md`](docs/releasing.md).

Initial release phải công bố rõ trạng thái ký, commit/run nguồn và SHA-256 theo
[`Initial release checklist`](docs/INITIAL_RELEASE_CHECKLIST.md). Repository hiện
chưa có GitHub Release.

## Đóng góp

Xem [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

Mã nguồn CADInspect được phát hành theo [MIT License](LICENSE). Qt,
OpenCASCADE và các dependency đi kèm giữ license riêng; xem
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
