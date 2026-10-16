// Issue #300: ui/win32 の描画の保持（字体選択 ADR 0071・字形 ADR 0073）の契約を
// 窓を作らずに確かめる（ADR 0077）。
// OS 資源は DirectWrite / WIC とメモリ上の DROPFILES（ADR 0116）。
// 窓・device・swap chain・GPU・時計・乱数・スレッド・ファイルを使わない。
// フォントに結果が依る検査は「環境依存」で、測れないときは 1 行言って数えない。
#include "WindowChecks.hpp"

#include <array>
#include <cstdio>
#include <objbase.h>
#include <string_view>
#include <utility>

namespace
{
bool verify_selected(std::string_view selector)
{
    namespace ui = nenenib::tests::ui;
    constexpr std::array<std::pair<std::string_view, void (*)()>, 8> groups{{
        {"", ui::verify_fallback_keys},
        {"", ui::verify_fallback_cache},
        {"--glyphs", ui::verify_glyph_collector},
        {"--glyphs", ui::verify_glyph_pixels},
        {"", ui::verify_gutter_layouts},
        {"--text-layout", ui::verify_body_text_layouts},
        {"--file-drop", ui::verify_file_drop},
        {"--grid", ui::verify_grid_render},
    }};
    bool known = false;
    for (const auto &[name, verify] : groups)
    {
        if (selector.empty() || selector == name)
        {
            verify();
            known = true;
        }
    }
    return known;
}
} // namespace

int main(int argc, char **argv)
{
    namespace ui = nenenib::tests::ui;
    // WIC の factory を作るために COM を初期化する（描画先はメモリの bitmap だけ）。
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ui::expect(SUCCEEDED(initialized), "COM is initialized for the software target");
    const bool arguments_known = argc == 1 || (argc == 2 && !std::string_view(argv[1]).empty());
    ui::expect(arguments_known && verify_selected(argc == 2 ? argv[1] : ""),
               "window verification selector is known");
    if (SUCCEEDED(initialized))
    {
        CoUninitialize();
    }
    std::printf("Window: %zu checks, %zu failures, %zu not measured\n", ui::checks(),
                ui::failures(), ui::unmeasured_count());
    return ui::failures() == 0 ? 0 : 1;
}
