#pragma once

#include "VimMapMode.hpp"

#include <utility>

namespace nenenib::core
{
// 設定の `map.<modes>.` の文字の集合（ADR 0176 の決定 1）。`n` `i` `v` `x` `o` の 5 つで、
// Nib では `v` と `x` は同じ VISUAL に効く。書き戻しで文字を変えないよう 2 つを分けて持つ。
// 値は単独で妥当な公開 aggregate で、比較と問いは非メンバー（CPP-003）。
struct VimModeSet
{
    bool normal = false;
    bool insert = false;
    bool visual = false;
    bool visual_only = false;
    bool operator_pending = false;
};

[[nodiscard]] constexpr bool operator==(VimModeSet left, VimModeSet right) noexcept
{
    return left.normal == right.normal && left.insert == right.insert &&
           left.visual == right.visual && left.visual_only == right.visual_only &&
           left.operator_pending == right.operator_pending;
}

[[nodiscard]] constexpr bool mode_set_empty(VimModeSet modes) noexcept
{
    return !modes.normal && !modes.insert && !modes.visual && !modes.visual_only &&
           !modes.operator_pending;
}

// 集合が mode に効くか。`v` と `x` はどちらも VISUAL。
[[nodiscard]] constexpr bool mode_set_contains(VimModeSet modes, VimMapMode mode) noexcept
{
    switch (mode)
    {
    case VimMapMode::normal:
        return modes.normal;
    case VimMapMode::insert:
        return modes.insert;
    case VimMapMode::visual:
        return modes.visual || modes.visual_only;
    case VimMapMode::operator_pending:
        return modes.operator_pending;
    }
    std::unreachable();
}
} // namespace nenenib::core
