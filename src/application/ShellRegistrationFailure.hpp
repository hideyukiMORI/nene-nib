#pragma once

#include <cstdint>

namespace nenenib::application
{
// エクスプローラーへの登録を書けない・消せない理由（ADR 0178 の決定 1・ARC-010）。拒否は
// access_denied、ほかの OS の失敗は io_error。例外にしない。
enum class ShellRegistrationFailure : std::uint8_t
{
    access_denied,
    io_error
};
} // namespace nenenib::application
