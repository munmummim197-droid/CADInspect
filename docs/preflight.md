# Build preflight

Tài liệu public chỉ ghi các yêu cầu tái lập cần thiết và không lưu fingerprint của
máy phát triển, VM, security policy hoặc đường dẫn riêng của maintainer.

## Yêu cầu tối thiểu

- Repository root: `<repository-root>`.
- Windows 10/11 x64.
- Visual Studio 2022 trở lên với MSVC x64 và Windows SDK.
- CMake 3.28 trở lên.
- Git và vcpkg; `VCPKG_ROOT` phải trỏ tới một checkout vcpkg hợp lệ.
- Khả năng build C++20 và chạy test executable trong workspace.

Manifest `vcpkg.json` ghim dependency baseline. Preset OSS dùng dynamic triplet
`x64-windows` và bật manifest install. Không cần thay đổi Defender, Smart App
Control, WDAC, AppLocker, certificate store hoặc system ACL để build project.

## Build kiểm chứng

```powershell
$env:VCPKG_ROOT = 'C:\path\to\vcpkg'
cmake --preset oss-release
cmake --build --preset oss-release
ctest --preset oss-release
```

Wrapper `scripts/Invoke-StepCompareBuild.ps1` chuẩn hóa môi trường chỉ cho tiến
trình build con trên các host có hai biến `Path`/`PATH`; script không sửa cấu hình
máy hoặc user environment.
