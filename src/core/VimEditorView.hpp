#pragma once

#include "Offset.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimDisplayNavigation.hpp"
#include "VimKeySource.hpp"
#include "VimLayoutView.hpp"
#include "VimMarks.hpp"
#include "VimPatternCase.hpp"
#include "VimRowLanding.hpp"
#include "VimTabs.hpp"
#include "VimViewport.hpp"

#include <optional>

namespace nenenib::core
{
// Vim の 1 打鍵が読む editor の現在値。本文と選択は借用し、表示領域は値で受ける。
// どれも保存せず、状態の所有者を増やさない（ADR 0019 の決定 1）。表示領域は engine が
// VimStep.viewport_request で求めたときだけ渡す（ADR 0129 の決定 1）。
struct VimEditorView
{
    const TextBuffer &text;
    const Selection &selection;
    std::optional<VimViewport> viewport;
    // 鍵の出どころ（ADR 0046 の決定 2）。controller が再生の中だけ replayed にする。
    VimKeySource source = VimKeySource::typed;
    // タブの本数と今の位置（ADR 0057 の決定 2）。既定はタブ 1 本の今の位置 0。
    VimTabs tabs = VimTabs{0, 1};
    VimLayoutView layout{};
    std::optional<VimDisplayNavigation> navigation = std::nullopt;
    // `gj` `gk` の着地（ADR 0144 の決定 4）。engine が鍵を写す前に画面行の問いで解いて置く。
    std::optional<VimRowLanding> row_landing = std::nullopt;
    // 文書のマーク（ADR 0152 の決定 2）。controller が今の文書の値を貸し、engine は読むだけで、
    // 変えるときは VimStep.marks に新しい値を添える。null にはしない（既定は何も置いていない値）。
    const VimMarks *marks = &no_vim_marks;
    // マークへの移動の着地（決定 3）。`'` `` ` `` の次の鍵で engine が名前を解いて置く。
    std::optional<Offset> mark_landing = std::nullopt;
    // 検索の大小の旗（設定の `ignorecase` `smartcase`・ADR 0032 の追補・#1041）。controller が
    // 設定から置き、`/` `?` の Enter と `*` `#` が新しい検索を解くときに読む。
    VimPatternCase search_case = VimPatternCase::sensitive;
};
} // namespace nenenib::core
