#pragma once

#include <cstdint>

namespace nenenib::core
{
// 文字コードを判別できなかったこと。期待される失敗なので結果型で返す（ARC-010 / CPP-005）。
// UTF-16 の BOM の後ろが奇数バイトなら odd_utf16_length（ADR 0149 の決定 2）。
// UTF-32 の BOM（FF FE 00 00 / 00 00 FE FF）なら unsupported_utf32（ADR 0149 の決定 1・#734）。
enum class EncodingFailure : std::uint8_t
{
    undecodable,
    odd_utf16_length,
    unsupported_utf32
};
} // namespace nenenib::core
