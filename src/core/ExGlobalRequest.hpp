#pragma once

#include "ExLinesRequest.hpp"
#include "ExRange.hpp"
#include "ExSubstituteRequest.hpp"

#include <optional>
#include <string>
#include <variant>

namespace nenenib::core
{
// `:g` が当たった行ごとに流す命令（ADR 0168 の決定 1・段 3a）。段 3a が受けるのは範囲の無い
// `:d [x] [count]`（verb は delete_lines だけ）と、範囲と回数の無い `:s` の族だけで、ほかは
// evaluate_ex が断る。閉じた和型なので、受ける命令が増えたらコンパイルが落ちる（CPP-002）。
using ExGlobalCommand = std::variant<ExLinesRequest, ExSubstituteRequest>;

// `:[range]g/{pat}/{cmd}`・`:g!`・`:v` の 1 命令（ADR 0168 の決定 1・5・段 3a）。evaluate_ex が
// 字面だけを読んだ値で、範囲を行に解いて当たる行を 1 回で決め、命令を流すのは engine の
// vim_global の 1 本。range が空なら本文の全体（Vim の `:g` の既定は `%`）。inverted は `:g!` と
// `:v`（当たらない行）。pattern は区切りの間の字面のまま（空なら最後に使ったパターン）、
// delimiter は区切りの文字。どの値も単独で妥当なので公開 aggregate（CPP-003）。
struct ExGlobalRequest
{
    std::optional<ExRange> range;
    bool inverted;
    char delimiter;
    std::string pattern;
    ExGlobalCommand command;
};
} // namespace nenenib::core
