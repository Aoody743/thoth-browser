# Building Thoth Browser on Windows

Thoth's Windows port builds the Qt shell and the Ladybird engine from source with
MSVC/clang-cl. The result is `Build/release/bin/Thoth.exe` plus the helper
processes (`WebContent.exe`, `RequestServer.exe`, `Compositor.exe`, ...).

This document describes the configuration used for the Windows alpha builds and
the adaptations that are currently in the tree.

## Prerequisites

- Windows 10 21H2 or newer, x64
- Visual Studio 2022 Build Tools with the C++ workload and a Windows 11 SDK
- LLVM 20.x (`clang-cl`, `lld-link`) on `PATH`
- CMake 3.30 or newer and Ninja
- Rust 1.98 with the `x86_64-pc-windows-msvc` target
- Python 3.12
- Qt 6.10.3 for MSVC 2022 x64 (official binaries are fine; Qt Positioning is optional)
- vcpkg (Thoth's `Meta/ladybird.py vcpkg` bootstraps it from `vcpkg.json`)

## Configure and build

Run these from a shell where `vcvars64.bat` has been sourced:

```bat
set VCPKG_ROOT=%CD%\Build\vcpkg-release
set PATH=<qt>\6.10.3\msvc2022_64\bin;%PATH%

cmake --preset Release -S . -B Build/release ^
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
  -DCMAKE_PREFIX_PATH=<qt>\6.10.3\msvc2022_64 ^
  -DPython3_EXECUTABLE=<python3.12>\python.exe ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCMAKE_EXE_LINKER_FLAGS=/force:multiple ^
  -DLADYBIRD_WINDOWS_DIRECT3D=OFF

cmake --build Build/release --target ladybird --parallel 6
```

`BUILD_SHARED_LIBS=OFF` is required today: several libraries do not export their
template instantiations from a DLL, so a static build is used. `/force:multiple`
masks duplicated Rust standard-library symbols that the static link otherwise
reports.

`LADYBIRD_WINDOWS_DIRECT3D` selects the presentation path:

- `OFF` (default) paints web content through Qt's widget backing store. This is
  the most compatible mode and is what Windows alpha builds ship with.
- `ON` presents frames through a `QRhiWidget` with Direct3D 11. Faster, but
  requires a working D3D11 driver and is still experimental.

## Running

```bat
set PATH=%CD%\Qt\6.10.3\msvc2022_64\bin;%PATH%
cd Build\release
bin\Thoth.exe --profile-path C:\path\to\profile --disable-sandbox https://example.com
```

`--disable-sandbox` is needed until the helper-process sandbox is implemented on
Windows. A portable folder can be assembled with `package_windows.ps1`.

## Windows adaptations in this tree

- `UI/CMakeLists.txt` names the browser executable `Thoth.exe` on Windows.
- `Meta/CMake/check_for_dependencies.cmake` adds `LADYBIRD_WINDOWS_DIRECT3D`
  (off by default) and skips the Direct3D presentation path when disabled.
- `Meta/CMake/compile_options.cmake` keeps C++ and Rust on the baseline x64
  instruction set on Windows so builds are portable between machines.
- `UI/Qt/Thoth/Appearance.cpp` exposes the chrome background color so the
  toolbar row and the frameless window's drag strip are painted with Thoth's
  palette instead of the default black client area.
- `Libraries/LibTLS/TLSv12.cpp` loads the Windows `ROOT` certificate store with
  WinCrypt when OpenSSL's `winstore` provider is unavailable.
- `Libraries/LibCompositing` and `Libraries/LibWeb` skip the Unix `ar -x/-qS`
  archive merge on Windows.
- `vcpkg.json` does not install `qtbase`/`qtpositioning` on Windows; the Qt SDK
  selected with `CMAKE_PREFIX_PATH` is used instead.

## Known limitations

- Helper processes run without a sandbox on Windows.
- Site isolation and the accessibility bridge are not implemented for Windows.
- Crash reporting is not implemented for Windows.
- CSS-animated content (for example Baidu's hot-search marquee) can leave frame trails until the next full repaint.
- System certificate-store access can be denied in restricted service/AppContainer contexts; set `SSL_CERT_FILE` to a CA bundle there.

