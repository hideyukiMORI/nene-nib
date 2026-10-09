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
    src/core/ThemeName.cpp src/core/ThemeDocument.cpp src/core/ThemeChoice.cpp
    src/core/ThemeCatalog.cpp src/core/ThemeLookupFailure.cpp src/core/ExEvaluationFailure.cpp
    src/core/BookmarkKey.cpp src/core/BodyLayout.cpp src/core/CaretMove.cpp src/core/ClipboardText.cpp src/core/CommandLayout.cpp
    src/core/CommandLine.cpp src/core/CommandChoice.cpp src/core/CommandPalette.cpp src/core/PaletteMarks.cpp src/core/Composition.cpp
    src/core/DisplayLine.cpp src/core/DisplayText.cpp src/core/EditorSettings.cpp src/core/ExResult.cpp src/core/FontSize.cpp
    src/core/EditHistory.cpp src/core/FileFolder.cpp src/core/FilePath.cpp src/core/InputText.cpp
    src/core/KeyChord.cpp src/core/ModeLabel.cpp src/core/OperationBindings.cpp src/core/OperationGuide.cpp src/core/OperationGuideLayout.cpp src/core/OperationTexts.cpp src/core/OrdinaryCharacterBoundary.cpp src/core/Palette.cpp src/core/PaletteLayout.cpp
    src/core/ScrollBounds.cpp src/core/SearchLine.cpp
    src/core/StatusBarLayout.cpp src/core/StatusItems.cpp
    src/core/TabDestination.cpp src/core/TabKeyTable.cpp src/core/TabRecency.cpp src/core/TabTitle.cpp src/core/TextBuffer.cpp src/core/TextEncoding.cpp
    src/core/TitleBarLayout.cpp src/core/UnlistedExtensions.cpp src/core/Utf16.cpp src/core/Utf8.cpp
    src/core/VimBlockRange.cpp src/core/VimCaret.cpp src/core/VimCharacterBoundary.cpp src/core/VimClipboardText.cpp src/core/VimKeyTable.cpp src/core/VimPattern.cpp src/core/VimRegisterText.cpp
    src/core/VimSearch.cpp src/core/VimSearchNotice.cpp
    src/core/VimStep.cpp src/core/VimTextObjectRange.cpp
    src/core/VimVisualRange.cpp src/core/VimVisualReselect.cpp
    src/core/VimWordMotion.cpp src/core/VirtualColumn.cpp)

nenenib_target(nenenib_application application STATIC
    src/application/SettingsIssue.cpp src/application/CommandInput.cpp src/application/EditorController.cpp src/application/EditorState.cpp
    src/application/FileBookmarkEdit.cpp src/application/FileHistoryEdit.cpp src/application/ImeStance.cpp)
nenenib_link(nenenib_application nenenib_core)

nenenib_target(nib_tests unit_tests EXECUTABLE
    tests/unit/NibTests.cpp
    tests/unit/TestSupport.cpp
    tests/unit/VimTestSupport.cpp
    tests/unit/CoreTests.cpp
    tests/unit/ApplicationTests.cpp
    tests/unit/EditorDeliveryTests.cpp
    tests/unit/VimEngineTests.cpp
    tests/unit/DisplayLineTests.cpp
    tests/unit/VimDotTests.cpp
    tests/unit/VimMacroTests.cpp
    tests/unit/VimClipboardTests.cpp
    tests/unit/VimSearchTests.cpp
    tests/unit/VimSearchHighlightTests.cpp
    tests/unit/VimSearchIncrementalTests.cpp
    tests/unit/VimTextObjectTests.cpp
    tests/unit/VimReplaceTests.cpp
    tests/unit/VimVisualYankTests.cpp
    tests/unit/VimVisualWantedTests.cpp
    tests/unit/VimOpenLineTests.cpp
    tests/unit/VimOpenLineExternalTests.cpp
    tests/unit/VimOpenLineRecoveryTests.cpp
    tests/unit/VimCharacterSearchTests.cpp
    tests/unit/VimLineJumpTests.cpp
    tests/unit/VimLineJumpRecoveryTests.cpp
    tests/unit/VimVirtualColumnTests.cpp
    tests/unit/VimCharacterBoundaryTests.cpp
    tests/unit/UserThemeSelectionTests.cpp
    tests/unit/UserThemeValuesTests.cpp
    tests/unit/CommandPaletteTests.cpp
    tests/unit/ExSettingsTests.cpp
    tests/unit/ExFileTests.cpp
    tests/unit/ExDocumentTests.cpp
    tests/unit/ExWritePathTests.cpp
    tests/unit/OrdinaryCharacterTests.cpp
    tests/unit/VimVisualBlockTests.cpp
    tests/unit/TabsTests.cpp
    tests/unit/SessionTests.cpp
    tests/unit/BookmarkTests.cpp
    tests/unit/HistoryTests.cpp
    tests/unit/BackgroundWorkTests.cpp
    tests/unit/OperationsTests.cpp
    tests/unit/GuideTests.cpp)
nenenib_link(nib_tests nenenib_core)
nenenib_link(nib_tests nenenib_application)
add_test(NAME nib_unit COMMAND nib_tests)

nenenib_target(nenenib_adapters adapters_win32 STATIC
    src/adapters/win32/KeyValueFields.cpp src/adapters/win32/ColorCodec.cpp
    src/adapters/win32/ThemeCodec.cpp src/adapters/win32/ThemeContrast.cpp
    src/adapters/win32/Win32ThemeAdapter.cpp
    src/adapters/win32/AbsolutePath.cpp src/adapters/win32/LocalSettingsPath.cpp
    src/adapters/win32/SettingsCodec.cpp src/adapters/win32/Win32SettingsAdapter.cpp
    src/adapters/win32/ParentDirectory.cpp src/adapters/win32/SessionCodec.cpp
    src/adapters/win32/Win32SessionAdapter.cpp src/adapters/win32/TextLines.cpp
    src/adapters/win32/BookmarkCodec.cpp src/adapters/win32/Win32BookmarkAdapter.cpp
    src/adapters/win32/HistoryCodec.cpp src/adapters/win32/Win32HistoryAdapter.cpp
    src/adapters/win32/Win32AppearanceAdapter.cpp
    src/adapters/win32/Win32ClipboardAdapter.cpp src/adapters/win32/Win32CodePageAdapter.cpp
    src/adapters/win32/Win32FileAdapter.cpp src/adapters/win32/Win32TimingAdapter.cpp
    src/adapters/win32/Win32Worker.cpp src/adapters/win32/FolderShelf.cpp
    src/adapters/win32/FolderListing.cpp src/adapters/win32/Win32FolderAdapter.cpp)
nenenib_link(nenenib_adapters nenenib_core)
nenenib_link(nenenib_adapters nenenib_application)
# RegGetValueW（OS のテーマ読み取り）だけ。ここが advapi32 を結ぶ唯一のモジュール。
nenenib_system_link(nenenib_adapters advapi32)
# クリップボード（OpenClipboard / SetClipboardData）は user32。Issue #7 で足した唯一の OS 依存。
nenenib_system_link(nenenib_adapters user32)

# QLT-013: adapters/win32 のファイル読み書きは、ビルドディレクトリ配下の一時ファイルで往復させる
# （ADR 0010）。中核の分岐カバレッジ（coverage-policy.json）の対象には入れない。
nenenib_target(nib_adapter_tests adapter_tests EXECUTABLE tests/adapters/NibAdapterTests.cpp)
nenenib_link(nib_adapter_tests nenenib_core)
nenenib_link(nib_adapter_tests nenenib_application)
nenenib_link(nib_adapter_tests nenenib_adapters)
add_test(NAME nib_adapters COMMAND nib_adapter_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

nenenib_target(nib_theme_tests adapter_tests EXECUTABLE tests/adapters/ThemeCodecTests.cpp)
nenenib_link(nib_theme_tests nenenib_core)
nenenib_link(nib_theme_tests nenenib_application)
nenenib_link(nib_theme_tests nenenib_adapters)
add_test(NAME nib_themes COMMAND nib_theme_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

nenenib_target(nib_session_tests adapter_tests EXECUTABLE tests/adapters/SessionAdapterTests.cpp)
nenenib_link(nib_session_tests nenenib_core)
nenenib_link(nib_session_tests nenenib_application)
nenenib_link(nib_session_tests nenenib_adapters)
add_test(NAME nib_sessions COMMAND nib_session_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

nenenib_target(nib_bookmark_tests adapter_tests EXECUTABLE tests/adapters/BookmarkAdapterTests.cpp)
nenenib_link(nib_bookmark_tests nenenib_core)
nenenib_link(nib_bookmark_tests nenenib_application)
nenenib_link(nib_bookmark_tests nenenib_adapters)
add_test(NAME nib_bookmarks COMMAND nib_bookmark_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

nenenib_target(nib_history_tests adapter_tests EXECUTABLE tests/adapters/HistoryAdapterTests.cpp)
nenenib_link(nib_history_tests nenenib_core)
nenenib_link(nib_history_tests nenenib_application)
nenenib_link(nib_history_tests nenenib_adapters)
add_test(NAME nib_histories COMMAND nib_history_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Issue #271: 同じフォルダの列挙とワーカー 1 本を本物のフォルダとスレッドで測る（ADR 0062）。時刻は読まず、
# 合図の受け皿だけを待つので、止まらないときはこの制限時間で落ちる。
nenenib_target(nib_folder_tests adapter_tests EXECUTABLE tests/adapters/FolderAdapterTests.cpp)
nenenib_link(nib_folder_tests nenenib_core)
nenenib_link(nib_folder_tests nenenib_application)
nenenib_link(nib_folder_tests nenenib_adapters)
add_test(NAME nib_folders COMMAND nib_folder_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(nib_folders PROPERTIES TIMEOUT 60)

nenenib_target(nenenib_window ui_win32 STATIC
    src/ui/win32/Direct2DRenderer.cpp src/ui/win32/EditorWindow.cpp src/ui/win32/FileDialog.cpp
    src/ui/win32/FontFallbackCache.cpp src/ui/win32/FontFallbackKey.cpp
    src/ui/win32/BodyGlyphCollector.cpp)
nenenib_link(nenenib_window nenenib_core)
nenenib_link(nenenib_window nenenib_application)
# ole32: IFileOpenDialog / IFileSaveDialog（ADR 0010 の決定 10）。ダイアログは窓の一部である。
# imm32: IMM32 の変換文字列と候補窓（ADR 0014 の決定 1）。IME は窓への入力なので ui/win32 に置く。
foreach(library IN ITEMS user32 d2d1 dwrite dxgi d3d11 dcomp dwmapi imm32 ole32)
    nenenib_system_link(nenenib_window ${library})
endforeach()

# Issue #300: ui/win32 の描画の保持（字体選択 ADR 0071・字形 ADR 0073）の契約を窓を作らずに確かめる
# （ADR 0077）。OS の資源は DirectWrite の factory と WIC のソフトウェアの描画先まで。
nenenib_target(nib_window_tests window_tests EXECUTABLE
    tests/ui/NibWindowTests.cpp tests/ui/WindowChecks.cpp tests/ui/DirectWriteFactory.cpp
    tests/ui/ScriptedTextSource.cpp tests/ui/ScriptedFontFallback.cpp tests/ui/SoftwareTarget.cpp
    tests/ui/FallbackKeyTests.cpp tests/ui/FallbackCacheTests.cpp
    tests/ui/GlyphCollectorTests.cpp tests/ui/GlyphPixelTests.cpp)
nenenib_link(nib_window_tests nenenib_core)
nenenib_link(nib_window_tests nenenib_window)
foreach(library IN ITEMS ole32 d2d1 dwrite windowscodecs)
    nenenib_system_link(nib_window_tests ${library})
endforeach()
add_test(NAME nib_window COMMAND nib_window_tests)

nenenib_target(NeNeNib app EXECUTABLE src/app/Main.cpp src/app/NeNeNib.rc)
set_source_files_properties("${NENENIB_MANIFEST}" PROPERTIES GENERATED TRUE)
target_sources(NeNeNib PRIVATE "${NENENIB_MANIFEST}")
target_include_directories(NeNeNib PRIVATE "${NENENIB_GENERATED_DIR}")
nenenib_link(NeNeNib nenenib_core)
nenenib_link(NeNeNib nenenib_application)
nenenib_link(NeNeNib nenenib_adapters)
nenenib_link(NeNeNib nenenib_window)
nenenib_system_link(NeNeNib user32)
# ole32: CoInitializeEx（合成ルートで 1 回）。shell32: CommandLineToArgvW（ADR 0010 の決定 10・11）。
nenenib_system_link(NeNeNib ole32)
nenenib_system_link(NeNeNib shell32)
set_property(TARGET NeNeNib PROPERTY WIN32_EXECUTABLE TRUE)

# ADR 0082: 開発用の閉じた比較。unit は inline の足場だけ借用し、CTest に登録しない。
nenenib_target(nib_perf_probes performance_tests EXECUTABLE
    tests/performance/ProbeMain.cpp tests/performance/ProbeWorkload.cpp)
nenenib_link(nib_perf_probes nenenib_core)
nenenib_link(nib_perf_probes nenenib_application)
nenenib_link(nib_perf_probes nenenib_adapters)
target_include_directories(nib_perf_probes PRIVATE "${CMAKE_SOURCE_DIR}/tests/unit")
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE NIB_PROBE_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" status --porcelain --untracked-files=no
    OUTPUT_VARIABLE NIB_PROBE_DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
file(SHA256 "${CMAKE_SOURCE_DIR}/eng/tool-versions.json" NIB_PROBE_TOOLCHAIN_HASH)
target_compile_definitions(nib_perf_probes PRIVATE
    NIB_PROBE_COMMIT="${NIB_PROBE_COMMIT}" NIB_PROBE_DIRTY="$<BOOL:${NIB_PROBE_DIRTY}>"
    NIB_PROBE_COMPILER="${CMAKE_CXX_COMPILER_VERSION}" NIB_PROBE_CONFIGURATION="$<CONFIG>"
    NIB_PROBE_TOOLCHAIN_HASH="${NIB_PROBE_TOOLCHAIN_HASH}")
