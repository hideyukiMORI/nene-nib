#pragma once

#include <cstdint>

namespace nenenib::core
{
// `gn` `gN` を打った所（Issue #1617）。NORMAL とオペレータ待ちはキャレットから選び始め、VISUAL は
// 今の選択を広げる（Vim の current_search の VIsual_active）。選択肢が増えたら写し先の switch が
// コンパイルで落ちる（CPP-002）。
enum class VimSearchObjectFrom : std::uint8_t
{
    caret,
    selection
};
} // namespace nenenib::core
