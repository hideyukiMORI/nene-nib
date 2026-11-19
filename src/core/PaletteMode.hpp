#pragma once

#include "ContextMenuMode.hpp"
#include "Distribution.hpp"
#include "EditMode.hpp"

namespace nenenib::core
{
// Ctrl+P の面を開いたときのモード（ADR 0078 の決定 11 と追補 #1051）。edit は使える操作を選び、
// menu は Vim の NORMAL / VISUAL で切り取り・コピー・貼り付け・削除・すべて選択の鍵の表示を Vim の
// 鍵の列にする（列の無い組み合わせは一覧に出さない）。distribution は製品の形で、Store 版で効か
// ない操作を一覧に出さない（ADR 0178 の追補 #1809）。
struct PaletteMode
{
    EditMode edit;
    ContextMenuMode menu;
    Distribution distribution;
};

// 編集モードだけを知る所（試験と測定）の面のモード。Vim は INSERT と同じ扱い（Vim の鍵の列を使わ
// ない）で、製品の形は ZIP 版。製品の面は controller が 3 つの値を名指しで作る。
[[nodiscard]] constexpr PaletteMode palette_mode_of(EditMode mode) noexcept
{
    return PaletteMode{
        mode, mode == EditMode::vim ? ContextMenuMode::vim_insert : ContextMenuMode::ordinary,
        Distribution::unpackaged};
}
} // namespace nenenib::core
