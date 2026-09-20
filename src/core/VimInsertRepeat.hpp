#pragma once

#include <cstddef>
#include <string>

namespace nenenib::core
{
// 回数付きo/OのINSERT中だけ持つ、残り回数とLFの入力記録（ADR 0028）。本文は所有しない。
struct VimInsertRepeat
{
    std::size_t remaining;
    std::string text;
};
} // namespace nenenib::core
