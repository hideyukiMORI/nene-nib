#pragma once

namespace nenenib::core
{
// 打たれた 1 文字。値は単独で妥当なので公開 aggregate で、比較は非メンバー（CPP-003）。
struct VimCharacter
{
    char32_t code;
};

[[nodiscard]] constexpr bool operator==(VimCharacter left, VimCharacter right) noexcept
{
    return left.code == right.code;
}
} // namespace nenenib::core
