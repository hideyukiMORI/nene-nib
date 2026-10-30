// Issue #300: ui/win32 の描画の保持（字体選択 ADR 0071・字形 ADR 0073）の契約を
// 窓を作らずに確かめる（ADR 0077）。
// OS 資源は DirectWrite / WIC とメモリ上の DROPFILES（ADR 0116）。
// 窓・swap chain・時計・乱数・スレッド・ファイルを使わない。device を作るのは `--device-warmup`
// の組だけで（ADR 0131）、作れない機械では測らない。
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
    constexpr std::array<std::pair<std::string_view, void (*)()>, 22> groups{{
        {"", ui::verify_fallback_keys},
        {"", ui::verify_fallback_cache},
        {"--glyphs", ui::verify_glyph_collector},
        {"--glyphs", ui::verify_glyph_pixels},
        {"--fallback-ranges", ui::verify_fallback_ranges},
        {"--fallback-memo", ui::verify_fallback_segment_memo},
        {"--replaced", ui::verify_replaced_tints},
        {"", ui::verify_gutter_layouts},
        {"--text-layout", ui::verify_body_text_layouts},
        {"--file-drop", ui::verify_file_drop},
        {"--grid", ui::verify_grid_render},
        {"--milestones", ui::verify_presented_milestones},
        {"--device-warmup", ui::verify_device_warmup},
        {"--timers", ui::verify_window_timers},
        {"--wheel", ui::verify_wheel_turns},
        {"--preview", ui::verify_preview_layouts},
        {"--status", ui::verify_status_demand},
        {"--syntax", ui::verify_syntax_render},
        {"--palette", ui::verify_palette_layouts},
        {"--explorer", ui::verify_explorer_paint},
        {"--pointer", ui::verify_pointer_presses},
        {"--print", ui::verify_print_render},
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
