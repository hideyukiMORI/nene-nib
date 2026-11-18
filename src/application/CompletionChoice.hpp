#pragma once

#include <cstddef>

namespace nenenib::application
{
// Vim の INSERT の補完の面の行のクリック（#1845・ADR 0190 の決定 5）。鍵ではないので engine は
// vim_completion_chosen で受け、マクロの録画に積まない。row は面の候補の列の中の位置。
struct CompletionChoice
{
    std::size_t row;
};
} // namespace nenenib::application
