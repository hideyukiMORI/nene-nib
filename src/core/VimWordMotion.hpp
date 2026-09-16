#pragma once

#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "VimWordStop.hpp"

#include <cstddef>

namespace nenenib::core
{
// Vim の語の移動（ADR 0012 の決定 5）。語の切れ目は文字の種類（空白・記号・語の文字・
// ひらがな・カタカナ・漢字…）が変わるところで、空行そのものも 1 つの語として止まる。
// 既存の moved_caret の語は空白だけで切るので別物。あちらは通常モードのまま変えない。
[[nodiscard]] Offset vim_next_word(const TextBuffer &text, Offset caret, std::size_t count,
                                   VimWordStop stop);
[[nodiscard]] Offset vim_previous_word(const TextBuffer &text, Offset caret, std::size_t count);
} // namespace nenenib::core
