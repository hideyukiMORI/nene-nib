#pragma once

#include "ContextMenuMode.hpp"
#include "EditMode.hpp"

namespace nenenib::core
{
// Ctrl+P の面を開いたときのモード（ADR 0078 の決定 11 と追補 #1051）。edit は使える操作を選び、
// menu は Vim の NORMAL / VISUAL で切り取り・コピー・貼り付け・削除・すべて選択の鍵の表示を Vim の
// 鍵の列にする（列の無い組み合わせは一覧に出さない）。
struct PaletteMode
{
    EditMode edit;
    ContextMenuMode menu;
};

// 編集モードだけを知る所の面のモード。Vim は INSERT と同じ扱い（Vim の鍵の列を使わない）。
[[nodiscard]] constexpr PaletteMode palette_mode_of(EditMode mode) noexcept
{
    return PaletteMode{mode, mode == EditMode::vim ? ContextMenuMode::vim_insert
                                                   : ContextMenuMode::ordinary};
}
} // namespace nenenib::core
