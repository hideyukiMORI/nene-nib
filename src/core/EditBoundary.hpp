#pragma once

#include <cstdint>

namespace nenenib::core
{
// undo の単位の区切り方。通常モードでは連続した文字入力だけが coalesce で 1 つにまとまり、
// 移動・削除・貼り付け・改行は separate で区切る（ADR 0009 の決定 3）。
// absorb は Vim の INSERT のあいだだけ使い、削除も含めて直前の Edit に畳む（ADR 0015 の決定 5）。
enum class EditBoundary : std::uint8_t
{
    coalesce,
    separate,
    absorb
};
} // namespace nenenib::core
