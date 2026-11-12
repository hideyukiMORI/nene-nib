#pragma once

#include <cstdint>

namespace nenenib::core
{
// Ex の番地の基点（Vim の get_address の部分集合・ADR 0168 の決定 1）。current は `.` と、基点を
// 省いた番地（`:+2` `:,5` の空の側）で、どちらもキャレットの行（`;` の後ろは前の番地の行）。
// `/pat/` `?pat?` `\/` の基点は後続。
enum class ExAddressBase : std::uint8_t
{
    current,
    // `$`（最後の行）。
    last,
    // 10 進の行番号。
    number,
    // `'{name}`（マークの行）。
    mark
};
} // namespace nenenib::core
