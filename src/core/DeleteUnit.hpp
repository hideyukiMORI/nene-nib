#pragma once

#include <cstdint>

namespace nenenib::core
{
// 選択が空のときに消す大きさ（ADR 0009 の追補 #1394）。Backspace / Delete が character、
// 通常モードの Ctrl+Backspace / Ctrl+Delete が word（ordinary_word_boundary の着地まで）。
enum class DeleteUnit : std::uint8_t
{
    character,
    word
};
} // namespace nenenib::core
