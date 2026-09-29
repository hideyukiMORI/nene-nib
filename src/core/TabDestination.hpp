#pragma once

#include "TabJump.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// どのタブへ行くかを決める 1 本（ADR 0057 の決定 1）。Vim の `gt` `gT`・Ex の `tabnext`
// `tabprevious`・一覧の候補の実行がどれもここを通る（ARC-001）。窓の Ctrl+Tab は使った順
// （TabRecency・ADR 0058）で、ここを通らない。
// active は帯の上の今の位置、tab_count は本数（どちらも 0 始まりの帯の位置で数える）。
// 答えは行き先の帯の位置で、値なしは失敗（Vim の実測・out/probes/probe-vimtabs-2026-09-29.md）。
// - forward・回数なし: 次（末尾から先頭へ折り返す）。
// - forward・回数 N: N 番目（1 始まり）。N が 0 か本数より大きければ失敗。
// - backward・回数なし: 前（先頭から末尾へ折り返す）。
// - backward・回数 N: N 個ぶん戻る（折り返す）。N が 0 なら失敗。
// 本数 1 本でも同じ規則で、特別な枝は作らない。
[[nodiscard]] std::optional<std::size_t> tab_destination(const TabJump &jump, std::size_t active,
                                                         std::size_t tab_count) noexcept;
} // namespace nenenib::core
