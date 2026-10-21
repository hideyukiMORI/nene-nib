#pragma once

#include "GridCellSpan.hpp"
#include "GridColorSpan.hpp"
#include "LineNumber.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::application
{
// 格子の見えている 1 画面行の表示値（ADR 0140 の決定 5・ARC-011）。UI はこれを写すだけで、
// 本文にも升の数え方にも触らない。
//
// text は描く文字列で、Tab の升と送りの空きは空白、制御文字と書式用文字は `^M` `<200b>` の形
// （ADR 0040）。starts は text の code point ごとの始まりの升（大きさは code point 数 + 1 で、
// 末尾は升の和）。幅 2 の文字は 2 升、幅 0 の文字は 0 升。
// 範囲はどれもこの画面行の升で、本文の位置の範囲から application が写した値。画面行の境目を
// またぐ当たりや選択は、画面行ごとに 1 つずつの範囲になる。
struct GridRowView
{
    core::LineNumber number;
    // 行の中の画面行（0 始まり）と、本文の領域の上からの画面行。
    std::size_t row;
    std::int64_t screen_row;
    std::string text;
    std::vector<std::size_t> starts;
    std::optional<GridCellSpan> selection;
    // 選択が改行を越える行の最後の画面行で、行末の升の後ろに印を塗る。
    bool newline_selected;
    std::vector<GridCellSpan> matches;
    std::optional<GridCellSpan> current_match;
    // 置き換えて描いた文字（`^M` `<200b>`）の升。muted で描き直す（ADR 0040 の決定 4）。
    std::vector<GridCellSpan> replaced;
    // キャレットの升（縦棒は begin の左端・ブロックは範囲）。キャレットの画面行だけ値を持つ。
    std::optional<GridCellSpan> caret;
    // 変換中の文字列を差し込んだ text の code point の位置（ADR 0014 の決定 2）。差し込んだ行だけ。
    std::optional<std::size_t> composed_at;
    // 構文の色の升（#839・ADR 0156 の追補）。当たりと同じ写しで本文の位置から切り、変換中の文字列
    // を差し込んだ升は区間を割って色を付けない。
    std::vector<GridColorSpan> colors{};
};
} // namespace nenenib::application
