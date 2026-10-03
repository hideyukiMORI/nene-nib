#pragma once

#include "CaretMoveRequest.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"

#include <cstddef>

namespace nenenib::core
{
// 移動後のキャレット位置を返す純関数（ARC-007）。本文は変わらない。
// page_lines は PgUp / PgDn が動く行数で、見えている行数を application が渡す。
[[nodiscard]] Offset moved_caret(const TextBuffer &text, Offset caret, CaretMoveRequest request);
} // namespace nenenib::core
