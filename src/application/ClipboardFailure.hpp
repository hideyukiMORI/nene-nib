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
    read_failed,
    // 本文を OS の形（UTF-16）に変換できない。書かずに返す（ADR 0134 の決定 1）。
    unrepresentable
};
} // namespace nenenib::application
