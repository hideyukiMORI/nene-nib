#pragma once

#include "CommandChoice.hpp"
#include "DisplayText.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
struct CommandPaletteView
{
    // 絞り込みの結果の窓（ADR 0062 の決定 2・3）。rows の i 番目は結果の全体の first + i 番目で、
    // 件数は core::palette_row_limit を越えない。ui が描く行は必ずこの窓に入る。
    std::vector<core::CommandChoice> rows;
    // 窓の先頭・選択の位置・件数は、どれも結果の全体の中の数。
    std::size_t first;
    std::size_t selected;
    std::size_t total;
    // 検索欄の右の記号の案内（ADR 0060 の決定 9）。入力が空で、入力行が変換中でない
    // ときだけ値がある（変換を始めたら消え、取り消して空に戻ればまた出る）。
    // ui は値があれば描くだけ。
    std::optional<core::DisplayText> hint;
};
} // namespace nenenib::application
