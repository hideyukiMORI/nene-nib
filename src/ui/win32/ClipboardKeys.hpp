#pragma once

#include "ClipboardOperation.hpp"
#include "EditMode.hpp"
#include "VimMode.hpp"

#include <windows.h>

#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// OS のクリップボードへ貼り付けてよいモードか。通常モードと Vim の INSERT だけで、Vim の
// NORMAL / VISUAL では OS の貼り付けをしない（ADR 0035 の決定 9・ADR 0157 の決定 2 と同じ規則）。
// ui ではこの 1 本が、古いクリップボードの鍵と、Ctrl+V / Ctrl+Shift+V を Vim の矩形の鍵として
// 送るか（偽のとき送る）の両方を決める。controller の pastes_from_os は同じ規則の application 側。
[[nodiscard]] constexpr bool takes_os_clipboard_keys(core::EditMode mode,
                                                     core::VimMode vim) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return true;
    case core::EditMode::vim:
        break;
    }
    switch (vim)
    {
    case core::VimMode::insert:
        return true;
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return false;
    }
    std::unreachable();
}

// Windows の古いクリップボードの鍵（施主決定 D82・#1395・ADR 0009 の追補）。Ctrl+Insert はコピー、
// Shift+Insert は貼り付け、Shift+Delete は切り取り。それぞれ Ctrl+C / Ctrl+V / Ctrl+X と同じ規則で
// 効く: コピーはどのモードでも、貼り付けと切り取りは通常モードと Vim の INSERT だけ。
// Vim の NORMAL / VISUAL の Shift+Delete は今までどおり何も送らない（#1325 のレビュー L2）。
// ほかの鍵と修飾は空で、呼ぶ側は今までの表へ流す。Alt は見ない（Alt 単独は WM_SYSKEYDOWN で
// press_key に届かない。Ctrl+Alt+Insert（AltGr を含む）は Ctrl+Alt+C と同じくコピーになる）。
// 選択の無い Shift+Delete は切り取る本文が無いので何もしない（メモ帳の engine の RichEdit の実測と
// 同じ・EDIT は右の 1 文字を消す・PR #1422 の J1）。`os_keys` は
// `takes_os_clipboard_keys` の答え。窓を作らずに試せるようここに置く（nib_window_tests の
// `--standard-keys`）。
[[nodiscard]] constexpr std::optional<application::ClipboardOperation>
clipboard_key_for(WPARAM key, bool control, bool shifted, bool os_keys) noexcept
{
    if (key == VK_INSERT && control && !shifted)
    {
        return application::ClipboardOperation::copy;
    }
    if (control || !shifted || !os_keys)
    {
        return std::nullopt;
    }
    if (key == VK_INSERT)
    {
        return application::ClipboardOperation::paste;
    }
    if (key == VK_DELETE)
    {
        return application::ClipboardOperation::cut;
    }
    return std::nullopt;
}
} // namespace nenenib::ui::win32
