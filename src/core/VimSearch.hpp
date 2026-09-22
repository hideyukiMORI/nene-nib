#pragma once

#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchHit.hpp"

#include <optional>

namespace nenenib::core
{
// 1 回ぶんの検索（ADR 0032 の決定 3・4 の追記）。純関数で、本文・履歴・状態を持たない。
//
// 前向きは from の次のバイト以降で最初の一致、後ろ向きは from より前の最後の一致。
// 行ごとに 0 桁目から数え直し、必要な桁に足りない一致は終端（長さ 0 なら 1 文字先）まで
// 飛ばして数え直し、位置が行の長さに達したらその行を打ち切る（"aaaa" の /aa は 3 桁目、
// "abc" の /.* は動かない）。一致は行をまたがない。wrapscan は既定どおり有効で、
// 本文の端を越えて起点の行まで戻る。見つからなければ nullopt（呼ぶ側が E486 を出す）。
//
// 回数は呼ぶ側が着いた位置からもう一度呼んで数える（`3/x` と `/x` ＋ `3n` が同じ・実測）。
[[nodiscard]] std::optional<VimSearchHit> vim_search(const TextBuffer &text, Offset from,
                                                     const VimPattern &pattern,
                                                     VimSearchDirection direction);
} // namespace nenenib::core
