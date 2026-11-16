#pragma once

#include "CommandChoice.hpp"
#include "DisplayText.hpp"
#include "ProjectHitRow.hpp"
#include "ProjectSearchView.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
struct CommandPaletteView
{
    // 絞り込みの結果の窓（ADR 0062 の決定 2・3・ADR 0150 の決定 1）。rows の i 番目は結果の全体の
    // first + i 番目で、件数は面の見えている行数（core::palette_row_limit を越えない）。first は
    // 面の状態で、ui の描画とクリックはこの first を読み、計算し直さない。
    std::vector<core::CommandChoice> rows;
    // 窓の先頭・選択の位置・件数は、どれも結果の全体の中の数。
    std::size_t first;
    std::size_t selected;
    std::size_t total;
    // 検索欄の右の記号の案内（ADR 0060 の決定 9）。入力が空で、入力行が変換中でない
    // ときだけ値がある（変換を始めたら消え、取り消して空に戻ればまた出る）。
    // ui は値があれば描くだけ。
    std::optional<core::DisplayText> hint;
    // 全文検索の面（入力が `!` で始まる・ADR 0167 の決定 5）の進み具合。ほかの面では値なし。
    std::optional<ProjectSearchView> project = std::nullopt;
    // 全文検索の面の当たりの行の表示値。i 番目は rows の i 番目と同じ当たり（段 G3 が 2
    // 段で描く）。 ほかの面では空。
    std::vector<core::ProjectHitRow> hit_rows = {};
};
} // namespace nenenib::application
