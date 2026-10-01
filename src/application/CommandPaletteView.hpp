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
    std::vector<core::CommandChoice> choices;
    std::size_t selected;
    // 検索欄の右の記号の案内（ADR 0060 の決定 9）。入力が空で、入力行が変換中でない
    // ときだけ値がある（変換を始めたら消え、取り消して空に戻ればまた出る）。
    // ui は値があれば描くだけ。
    std::optional<core::DisplayText> hint;
};
} // namespace nenenib::application
