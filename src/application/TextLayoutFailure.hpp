#pragma once

#include <cstdint>
#include <utility>

namespace nenenib::application
{
// 文字組みの道の失敗（ADR 0133 の追補 2026-10-18・#665）。
// unavailable  … layout を作れない（device・保持の都合）。描けないので ui が描画を 1 回作り直す。
// inconsistent … engine と controller の問いの往復の契約の破れ。作り直しでは直らないので、鍵を
//                拒否して 1 行知らせる。配達に 1 回だけ載り、frame には載らない。
enum class TextLayoutFailure : std::uint8_t
{
    unavailable,
    inconsistent,
};

// frame に載せる（描けない）失敗か。
[[nodiscard]] constexpr bool blocks_drawing(TextLayoutFailure failure) noexcept
{
    switch (failure)
    {
    case TextLayoutFailure::unavailable:
        return true;
    case TextLayoutFailure::inconsistent:
        return false;
    }
    std::unreachable();
}
} // namespace nenenib::application
