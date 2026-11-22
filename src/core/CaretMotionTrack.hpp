#pragma once

#include "CaretQuad.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::core
{
// キャレットの動きの覚え（ADR 0191 の決定 2・3）。描く側が 1 つ持ち、frame ごとに
// caret_motion_followed が次の値を作る（純関数・時計は読まない）。target は前の frame の行き先、
// from は今の動きの出発の四隅、shown はこの frame に描く四隅、generation は動きの番号、moving は
// まだ着いていないか、bounds は出発の四隅を寄せる矩形（本文の領域・左上が corners[0]・右下が
// corners[2]・呼び手が frame ごとに置く）。公開 aggregate（ADR 0007）。
struct CaretMotionTrack
{
    std::optional<CaretQuad> target = std::nullopt;
    CaretQuad from{};
    CaretQuad shown{};
    std::uint32_t generation = 0;
    bool moving = false;
    CaretQuad bounds{};
};
} // namespace nenenib::core
