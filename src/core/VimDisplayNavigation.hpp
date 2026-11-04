#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "VimRepeatFailure.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 文字組みの値からengineが決めた画面移動。同じmotion/range/selection経路が消費する。
// 画面行の単位（ADR 0144 の決定 5）では先頭行の画面行first_rowと、キャレットの置き所placed
// （モードの寄せの前）を決め、caretの行とpreserve_columnは使わない。
struct VimDisplayNavigation
{
    LineNumber first;
    LineNumber caret;
    bool preserve_column = false;
    std::optional<std::size_t> scroll_lines = std::nullopt;
    std::size_t first_row = 0;
    std::optional<Offset> placed = std::nullopt;
    // H M L の裸の移動（オペレータの無いとき）の行。caret を余白の内側へ寄せた値で、オペレータの
    // 範囲は寄せない caret のまま（Vim の nv_scroll・#1099・ADR 0019 の追補）。
    std::optional<LineNumber> settled = std::nullopt;
    // 端で失敗した 1 画面の送り（Vim の onepage の FAIL・最初の周でも回数の途中でも）。窓と
    // キャレットは解いたとおりに動かし、engine が鍵の失敗の印にする（#1228・ADR 0019 の追補）。
    std::optional<VimRepeatFailure> failure = std::nullopt;
};
} // namespace nenenib::core
