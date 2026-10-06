// Issue #300: ui/win32 の描画の保持（字体選択 ADR 0071・字形 ADR 0073）の契約を
// 窓を作らずに確かめる（ADR 0077）。
// 持つ OS の資源は DirectWrite の factory と WIC のソフトウェアの描画先まで。
// 窓・device・swap chain・GPU・時計・乱数・スレッド・ファイルを使わない。
// フォントに結果が依る検査は「環境依存」で、測れないときは 1 行言って数えない。
#include "WindowChecks.hpp"

#include <cstdio>
#include <objbase.h>

int main()
{
    namespace ui = nenenib::tests::ui;
    // WIC の factory を作るために COM を初期化する（描画先はメモリの bitmap だけ）。
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ui::expect(SUCCEEDED(initialized), "COM is initialized for the software target");
    ui::verify_fallback_keys();
    ui::verify_fallback_cache();
    ui::verify_glyph_collector();
    ui::verify_glyph_pixels();
    if (SUCCEEDED(initialized))
    {
        CoUninitialize();
    }
    std::printf("Window: %zu checks, %zu failures, %zu not measured\n", ui::checks(),
                ui::failures(), ui::unmeasured_count());
    return ui::failures() == 0 ? 0 : 1;
}
