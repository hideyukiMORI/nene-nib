cmake_minimum_required(VERSION 3.31)
# 版はここが唯一の出どころ（Phase 4 で manifest / VERSIONINFO がこれを受け取る）。
project(NeNeNib VERSION 0.1.0 LANGUAGES CXX)

# manifest（PMv2 DPI・版）と VERSIONINFO は project(... VERSION) からだけ導く。
# 固定の manifest と版リテラルは CNF-007（eng/conformance.py の version_metadata_checks）が拒否する。
set(NENENIB_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${NENENIB_GENERATED_DIR}")
configure_file(src/app/NeNeNib.manifest.in "${NENENIB_GENERATED_DIR}/NeNeNib.manifest" @ONLY)
configure_file(src/app/NeNeNibVersion.h.in "${NENENIB_GENERATED_DIR}/NeNeNibVersion.h" @ONLY)
set(NENENIB_MANIFEST "${NENENIB_GENERATED_DIR}/NeNeNib.manifest")

# コンパイラは clang-cl ただ 1 つ（ADR 0003）。MSVC の toolset はリンカ・SDK・rc のためだけに入る。
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "QLT-011: this gate requires the pinned clang-cl compiler (CXX=clang-cl)")
endif()

# CMake 3.31 は clang-cl に -clang:-std=c++23 を渡す（Phase 0 CM1）。
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# CRT は常に /MT。Debug でも -MTd にしない（ASan が Debug CRT を支持しない・A1）。実行時依存 0（R1）。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
# CMake の既定は user32 / gdi32 / advapi32 等を全 target に結ぶので、platformLibraries（ARC-002）を
# 物理的な制約にするため kernel32 だけに絞る。OS ライブラリは nenenib_system_link でだけ足す。
set(CMAKE_CXX_STANDARD_LIBRARIES "kernel32.lib")

include(eng/targets.cmake)
# clang-tidy の cl ドライバは compile line の /clang: を落とすので、言語標準は extra-arg で同じ変数から渡す
# （NeNe Folio ADR 0003 の穴）。tests/build/ToolchainSmoke.cpp は C++23 専用構文だけで書いてあるので、
# tidy が C++17 で動いたらそこで落ちる。設定が在ることではなく、落ちることが検査である。
find_program(NENETIDY clang-tidy REQUIRED)
set(CMAKE_CXX_CLANG_TIDY "${NENETIDY};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=/clang:-std=c++${CMAKE_CXX_STANDARD}")

enable_testing()
nenenib_target(toolchain_smoke verification EXECUTABLE tests/build/ToolchainSmoke.cpp)
add_test(NAME toolchain_smoke COMMAND toolchain_smoke)

# Phase 3 の縦切り（Issue #3 / ADR 0007）。5 層に正典の経路を 1 本ずつ置く。
nenenib_target(nenenib_core core STATIC
    src/core/DisplayText.cpp src/core/ModeLabel.cpp src/core/Palette.cpp
    src/core/StatusBarLayout.cpp src/core/StatusItems.cpp src/core/TitleBarLayout.cpp)

nenenib_target(nenenib_application application STATIC
    src/application/EditorController.cpp src/application/EditorState.cpp)
nenenib_link(nenenib_application nenenib_core)

nenenib_target(nib_tests unit_tests EXECUTABLE tests/unit/NibTests.cpp)
nenenib_link(nib_tests nenenib_core)
nenenib_link(nib_tests nenenib_application)
add_test(NAME nib_unit COMMAND nib_tests)

nenenib_target(nenenib_adapters adapters_win32 STATIC
    src/adapters/win32/Win32AppearanceAdapter.cpp)
nenenib_link(nenenib_adapters nenenib_core)
nenenib_link(nenenib_adapters nenenib_application)
# RegGetValueW（OS のテーマ読み取り）だけ。ここが advapi32 を結ぶ唯一のモジュール。
nenenib_system_link(nenenib_adapters advapi32)

nenenib_target(nenenib_window ui_win32 STATIC
    src/ui/win32/Direct2DRenderer.cpp src/ui/win32/EditorWindow.cpp)
nenenib_link(nenenib_window nenenib_core)
nenenib_link(nenenib_window nenenib_application)
foreach(library IN ITEMS user32 d2d1 dwrite dxgi d3d11 dcomp dwmapi)
    nenenib_system_link(nenenib_window ${library})
endforeach()

nenenib_target(NeNeNib app EXECUTABLE src/app/Main.cpp src/app/NeNeNib.rc)
target_compile_definitions(NeNeNib PRIVATE "NENENIB_DISPLAY_TEXT=\"NeNe Nib ${PROJECT_VERSION}\"")
set_source_files_properties("${NENENIB_MANIFEST}" PROPERTIES GENERATED TRUE)
target_sources(NeNeNib PRIVATE "${NENENIB_MANIFEST}")
target_include_directories(NeNeNib PRIVATE "${NENENIB_GENERATED_DIR}")
nenenib_link(NeNeNib nenenib_core)
nenenib_link(NeNeNib nenenib_application)
nenenib_link(NeNeNib nenenib_adapters)
nenenib_link(NeNeNib nenenib_window)
nenenib_system_link(NeNeNib user32)
set_property(TARGET NeNeNib PROPERTY WIN32_EXECUTABLE TRUE)
