#pragma once

#include <cstdint>

namespace nenenib::application
{
// 登録の読み書きと上限の失敗。読めないものを空で上書きしない（ADR 0063）。
enum class FileBookmarksFailure : std::uint8_t
{
    location_unavailable,
    unreadable,
    too_large,
    malformed,
    unsupported_version,
    unwritable
};
} // namespace nenenib::application
