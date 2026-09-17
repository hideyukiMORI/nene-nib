#pragma once

#include "ClauseEmphasis.hpp"
#include "OffsetRange.hpp"

namespace nenenib::core
{
// 変換中の文字列の中の 1 文節（ADR 0014 の決定 2）。範囲は Composition の utf8 の中のバイト位置で、
// 本文のバイト位置ではない。どちらの値も単独で妥当なので公開 aggregate（CPP-003）。
struct CompositionClause
{
    OffsetRange range;
    ClauseEmphasis emphasis;
};

[[nodiscard]] constexpr bool operator==(const CompositionClause &left,
                                        const CompositionClause &right) noexcept
{
    return left.range == right.range && left.emphasis == right.emphasis;
}
} // namespace nenenib::core
