#pragma once

#include <cstdint>

namespace nenenib::application
{
// 閉じたファイルの履歴（history.v1）を読めない・書けない理由の閉じた集合（ADR 0060 の決定 8）。
// 版が違う・壊れている・大きすぎるものは、全体を読まなかったことにする。
enum class FileHistoryFailure : std::uint8_t
{
    location_unavailable,
    unreadable,
    too_large,
    malformed,
    unsupported_version,
    unwritable
};
} // namespace nenenib::application
