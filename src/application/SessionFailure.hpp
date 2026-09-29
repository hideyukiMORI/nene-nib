#pragma once

#include <cstdint>

namespace nenenib::application
{
// 前回のタブの一覧（session.v1）を読めない・書けない理由の閉じた集合（ADR 0059 の決定 2）。
// 版が違う・壊れている・大きすぎるものは、全体を読まなかったことにする。
enum class SessionFailure : std::uint8_t
{
    location_unavailable,
    unreadable,
    too_large,
    malformed,
    unsupported_version,
    unwritable
};
} // namespace nenenib::application
