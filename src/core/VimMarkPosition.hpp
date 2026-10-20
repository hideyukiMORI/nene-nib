#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// マークの位置（ADR 0152 の決定 2）。行と、行頭からのバイトの桁（0 始まり・Vim の pos_T の col）。
// Offset ではなく行と桁で持つのは、Vim のマークが行の挿入と削除では行で動き、行の中の編集では
// 桁を動かさないから（`:help mark_adjust`）。桁が行の内容を越えたら使う所で寄せる。どちらも単独で
// 妥当な値なので公開 aggregate（CPP-003）。
struct VimMarkPosition
{
    LineNumber line;
    std::size_t byte;
};

[[nodiscard]] constexpr bool operator==(const VimMarkPosition &left,
                                        const VimMarkPosition &right) noexcept
{
    return left.line == right.line && left.byte == right.byte;
}
} // namespace nenenib::core
