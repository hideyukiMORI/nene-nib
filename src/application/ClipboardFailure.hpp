#pragma once

#include <cstdint>

namespace nenenib::application
{
// クリップボードの失敗は期待される結果である（ARC-010 / CPP-005）。
enum class ClipboardFailure : std::uint8_t
{
    unavailable,
    empty,
    unsupported_format,
    write_failed,
    read_failed
};
} // namespace nenenib::application
