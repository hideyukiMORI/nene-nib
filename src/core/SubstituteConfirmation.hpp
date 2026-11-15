#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "SubstituteMatch.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::core
{
// `:s///c` の問いの途中（ADR 0168 の決定 6・段 2a）。engine の vim_substituted
// が当たりの列を計画して 最初の値を返し、答えのたびに vim_substitute_answered
// が次の値を返す。持つのは application の 入力行の状態（SubstituteQuestion）で、ui
// は意図を送るだけ。 matches は元の本文の当たりの列（共有して答えのたびに写さない）、next
// は問うている当たり、shift は next
// より前に置いた当たりが本文に足したバイト数（文書の改行の形・負なら減らした）。 substitutions と
// lines は置いた当たりの数と行の数（報せ）、counted_line は最後に数えた行。 shown
// は問いの文の置き換え（Vim の do_sub の sub・`~` を広げた後で `&` と `\1` はそのまま）、
// end_column は `$` の後の繰り返し（`a` の着地を行末にする）、breaks は置き換えが改行を入れるか。
// first_line は最初に置いた当たりの行、restore はその行の undo の戻り先（元の本文の位置・Vim の
// uh_cursor）。値は単独で妥当なので公開 aggregate（CPP-003）。
struct SubstituteConfirmation
{
    std::shared_ptr<const std::vector<SubstituteMatch>> matches;
    std::size_t next = 0;
    std::int64_t shift = 0;
    std::size_t substitutions = 0;
    std::size_t lines = 0;
    std::optional<LineNumber> counted_line = std::nullopt;
    std::string shown{};
    bool end_column = false;
    bool breaks = false;
    std::optional<LineNumber> first_line = std::nullopt;
    Offset restore{};
    // first_line の元の本文の行の頭と長さ（戻り先を行の中へ切り詰める）。
    Offset first_start{};
    std::size_t first_length = 0;
};
} // namespace nenenib::core
