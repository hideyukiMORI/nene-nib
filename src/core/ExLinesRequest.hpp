#pragma once

#include "ExLineVerb.hpp"
#include "ExRange.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// `:[range]d [x] [count]` と `:[range]y [x] [count]` の 1 命令（ADR 0168 の決定 1・5・段 3a）。
// evaluate_ex が字面だけを読んだ値で、範囲を行に解き、レジスタへ書き、行を消すのは engine の
// vim_lines_edited の 1 本。range が空なら今の行。register_name は打ったレジスタの名前（`a`〜`z`
// `A`〜`Z` `-` `_` `+` `*`・無ければ値なし）、count は末尾の回数（0 は engine が E939 で断る・
// 大きすぎる数は size_t の最大に飽和）。どの値も単独で妥当なので公開 aggregate（CPP-003）。
struct ExLinesRequest
{
    std::optional<ExRange> range;
    ExLineVerb verb;
    std::optional<char> register_name;
    std::optional<std::size_t> count;
};
} // namespace nenenib::core
