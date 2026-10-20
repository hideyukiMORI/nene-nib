#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// 1 つの編集が行へ与える形（ADR 0152 の決定 6・Vim が操作ごとに呼ぶ mark_adjust の引数に当たる）。
// 行はどれも編集の前の本文の行。first は編集の始まりの行、last は消した本文の終わりの行。
// [gone_first, gone_last] は消える行（gone_first > gone_last なら無し）。merges は last の残りが
// first へつながるか、joined はそのつなぎが J gJ（改行と字下げだけの削除）か。at_byte と end_byte
// は first と last の上の始まりと終わりの桁、inserted_bytes は入れた本文の長さ。removed_lines と
// inserted_lines は消した・入れた改行の数。pushes_first は入れた行が first のマークも押し下げるか
// （P O）。どの値も単独で妥当な公開 aggregate（CPP-003）。
struct VimMarkShift
{
    LineNumber first;
    LineNumber last;
    LineNumber gone_first;
    LineNumber gone_last;
    bool merges;
    bool joined;
    std::size_t at_byte;
    std::size_t end_byte;
    std::size_t inserted_bytes;
    std::size_t removed_lines;
    std::size_t inserted_lines;
    bool pushes_first;
};
} // namespace nenenib::core
