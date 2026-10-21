#pragma once

#include "NewlineDenseChunk.hpp"
#include "NewlineSparseChunk.hpp"

#include <variant>

namespace nenenib::core
{
// 改行の索引の 64 KiB の塊の表現は疎か密の 2 つだけ（ADR 0121 の追補の決定 2〜4）。読む所は
// std::visit で両方を書き、表現が増えたらコンパイルが落ちる。疎から密へは追記の 1 方向だけ。
using NewlineChunk = std::variant<NewlineSparseChunk, NewlineDenseChunk>;
} // namespace nenenib::core
