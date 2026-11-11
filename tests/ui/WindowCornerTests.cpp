#include "WindowChecks.hpp"
#include "WindowCorners.hpp"

namespace nenenib::tests::ui
{
// 窓の角の好み（#1488・ADR 0008 の決定 9 の追記）。値は DWMWCP_DONOTROUND で、伝える口は DWM の
// 属性 DWMWA_WINDOW_CORNER_PREFERENCE を試みて結果を返す（成否は OS の版に依るので製品は見ない）。
// 窓は作らない: 窓の無い呼び出しは DWM が失敗で返し、例外も止まりも無いことだけを見る。実際の窓の
// 角（DWM が合成した後の画素）は前面に出さないと撮れないので、施主の試用で確かめる。
void verify_window_corners()
{
    using nenenib::ui::win32::square_window_corners;
    using nenenib::ui::win32::window_corner_preference;
    expect(window_corner_preference == DWMWCP_DONOTROUND,
           "the window asks DWM not to round its corners");
    expect(FAILED(square_window_corners(nullptr)),
           "asking for square corners without a window fails quietly and returns");
}
} // namespace nenenib::tests::ui
