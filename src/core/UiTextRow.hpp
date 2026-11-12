#pragma once

#include "UiMessage.hpp"

#include <array>
#include <string_view>

namespace nenenib::core
{
// 文の表の 1 行（ADR 0123 の追補 2026-11-13 の決定 4）。text は UTF-8 で、差し込む値の置き場
// {0} {1} を持てる。
struct UiTextRow
{
    UiMessage message;
    std::string_view text;
};

[[nodiscard]] constexpr bool operator==(const UiTextRow &left, const UiTextRow &right) noexcept
{
    return left.message == right.message && left.text == right.text;
}

// 1 つの言語の列。UiMessage の順に全値を 1 回ずつ持つ（UiTexts.cpp の static_assert が守る）。
using UiTextColumn = std::array<UiTextRow, ui_message_count>;
} // namespace nenenib::core
