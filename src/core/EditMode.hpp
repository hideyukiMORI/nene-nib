#pragma once

#include <cstdint>
#include <utility>

namespace nenenib::core
{
// 編集モードの閉じた一覧。所有者は application の EditorState（ARC-004 / ADR 0008）。
enum class EditMode : std::uint8_t
{
    ordinary,
    vim
};

[[nodiscard]] constexpr EditMode toggled(EditMode mode) noexcept
{
    switch (mode)
    {
    case EditMode::ordinary:
        return EditMode::vim;
    case EditMode::vim:
        return EditMode::ordinary;
    }
    std::unreachable();
}
} // namespace nenenib::core
