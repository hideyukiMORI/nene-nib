#pragma once

#include <cstdint>

namespace nenenib::core
{
// undo の単位の区切り方。連続した文字入力だけが coalesce で 1 つにまとまり、
// 移動・削除・貼り付け・改行は separate で区切る（ADR 0009 の決定 3）。
enum class EditBoundary : std::uint8_t
{
    coalesce,
    separate
};
} // namespace nenenib::core
