// EditorWindow のエクスプローラーの面の群（ADR 0162 の決定 4・ADR 0158 の追補 7）。面に焦点が
// あるときの鍵の写し・面の上の押した行と離した行とダブルクリック・ホイール。面の状態と規則は
// application が持ち、ここは窓の入力を意図へ写すだけ。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "ExplorerKey.hpp"
#include "ExplorerKeyKind.hpp"
#include "ExplorerLayout.hpp"
#include "ExplorerPointing.hpp"
#include "ExplorerView.hpp"
#include "LayoutRect.hpp"
#include "PointExplorer.hpp"
#include "ScrollExplorer.hpp"
#include "WheelTurn.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
using application::ExplorerKeyKind;
using detail::high_word_of;
using detail::low_word_of;

// 面に焦点があるときの鍵の表（決定 4）。表にない鍵は捨てる（Vim の表と `map` の表を通さない）。
constexpr std::array<std::pair<WPARAM, ExplorerKeyKind>, 10> explorer_keys{{
    {'J', ExplorerKeyKind::j},
    {'K', ExplorerKeyKind::k},
    {'H', ExplorerKeyKind::h},
    {'L', ExplorerKeyKind::l},
    {VK_RETURN, ExplorerKeyKind::enter},
    {VK_ESCAPE, ExplorerKeyKind::esc},
    {VK_DOWN, ExplorerKeyKind::down},
    {VK_UP, ExplorerKeyKind::up},
    {VK_LEFT, ExplorerKeyKind::left},
    {VK_RIGHT, ExplorerKeyKind::right},
}};

[[nodiscard]] std::optional<ExplorerKeyKind> explorer_key_for(WPARAM key) noexcept
{
    for (const auto &[virtual_key, kind] : explorer_keys)
    {
        if (virtual_key == key)
        {
            return kind;
        }
    }
    return std::nullopt;
}
} // namespace

// 面に焦点があるときの鍵（WM_KEYDOWN）。操作の鍵（Ctrl+P・F1・Ctrl+Shift+E など）は press_key が
// 先に受けるので、ここへ来るのはそれ以外の鍵だけ。文字の鍵の WM_CHAR は type_character が捨てる。
void EditorWindow::press_explorer_key(WPARAM word)
{
    const auto kind = explorer_key_for(word);
    if (kind.has_value())
    {
        send(application::ExplorerKey{kind.value()});
    }
}

// 点の下の面の行（木の行の位置）。面の外なら値なし。面の先頭の行（ルート名）と行の無い所は
// 木の行の数（行の外・controller は何もしない・押した印は消す）。
std::optional<std::size_t> EditorWindow::explorer_row_at(LPARAM data) const
{
    const auto settings = controller_.delivery().settings;
    const auto layout = explorer_frame(settings);
    const auto x = low_word_of(data);
    const auto y = high_word_of(data);
    if (!layout.panel.has_value() || !core::contains(layout.panel.value(), x, y) ||
        layout.line_height <= 0)
    {
        return std::nullopt;
    }
    const auto view = controller_.explorer_view();
    if (!view.has_value())
    {
        return std::nullopt;
    }
    const auto line = static_cast<std::size_t>((y - layout.panel.value().top) / layout.line_height);
    if (line == 0 || line > view.value().rows.size())
    {
        return view.value().total;
    }
    return view.value().first_visible + line - 1;
}

// 面の上の押し（ADR 0056 の決定 9）。押した行は controller が節の経路で覚え、面に焦点を置く。
bool EditorWindow::press_explorer(LPARAM data)
{
    const auto row = explorer_row_at(data);
    if (!row.has_value())
    {
        return false;
    }
    explorer_pressed_ = true;
    send(application::PointExplorer{row.value(), application::ExplorerPointing::pressed});
    return true;
}

// 面の上で押した後の離し。離した点が面の外なら行の外として送る（押した印を消す）。
void EditorWindow::release_explorer(LPARAM data)
{
    if (!std::exchange(explorer_pressed_, false))
    {
        return;
    }
    const auto view = controller_.explorer_view();
    if (!view.has_value())
    {
        return;
    }
    const auto row = explorer_row_at(data).value_or(view.value().total);
    send(application::PointExplorer{row, application::ExplorerPointing::released});
}

bool EditorWindow::double_click_explorer(LPARAM data)
{
    const auto row = explorer_row_at(data);
    if (!row.has_value())
    {
        return false;
    }
    explorer_pressed_ = true;
    send(application::PointExplorer{row.value(), application::ExplorerPointing::double_click});
    return true;
}

void EditorWindow::scroll_explorer(std::int32_t delta)
{
    // 1 刻みの行数は OS の設定から。1 画面の設定なら本文の見えている行数。
    std::size_t visible = 0;
    if (wheel_setting_ == WHEEL_PAGESCROLL)
    {
        visible = active_body(controller_.delivery().settings).visible_lines;
    }
    const auto turn = wheel_turned(explorer_wheel_remainder_, delta,
                                   wheel_lines_per_notch(wheel_setting_, visible));
    explorer_wheel_remainder_ = turn.remainder;
    if (turn.lines == 0)
    {
        return;
    }
    send(application::ScrollExplorer{turn.lines});
}

bool EditorWindow::over_explorer(LPARAM data) const
{
    // WM_MOUSEWHEEL の位置は画面の座標なので、クライアントの座標に直してから見る。
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    const auto panel = explorer_frame(controller_.delivery().settings).panel;
    return panel.has_value() && core::contains(panel.value(), point.x, point.y);
}
} // namespace nenenib::ui::win32
