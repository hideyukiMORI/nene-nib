#pragma once

#include "VimBlockEdit.hpp"

#include <span>

namespace nenenib::application
{
// 整形の答えを組んだ編集の列（#1997 のレビュー R1・ADR 0191 の決定 7）。replace へ渡すと、マークと
// もう 1 つの枠を畳んだ 1 区間でなく編集ごとに直す（矩形の道の行ごとの列と違い、もう 1 つの枠も
// 編集ごとに写す）。pieces は編集の前の本文の上の重ならない区間を始まりの順に並べた列。公開
// aggregate（CPP-003）。
struct FormattingPieces
{
    std::span<const core::VimBlockEdit> pieces;
};
} // namespace nenenib::application
