#pragma once

#include <cstdint>

namespace nenenib::application
{
// CP932 ↔ UTF-8 の変換で起きる失敗（ADR 0010 の決定 2）。表せない文字を黙って '?' にしない。
enum class CodePageFailure : std::uint8_t
{
    undecodable,
    unencodable
};
} // namespace nenenib::application
