#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 範囲だけの Ex（ADR 0022 の追補 #1526）。NORMAL の回数つき `N:` が入力行に置く `:.` `:.,.+N` は
// キャレットの行から N 行下（line2）へ `{N}G` と同じ着地で移る。below は書いたままの N（大きすぎる
// 数は size_t の最大に飽和）で、行の番号に足して `:N` と同じ道へ渡すのは controller。VISUAL の
// `:` が置く `:'<,'>` は空で、キャレットを動かさない（Vim は VISUAL を抜けた選択の先の端に残す・
// 実測）。
struct ExRelativeLineRequest
{
    std::optional<std::size_t> below;
};
} // namespace nenenib::core
