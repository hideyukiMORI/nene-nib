#pragma once

#include "TextLayoutFailure.hpp"
#include "VimLineRows.hpp"
#include "VimRowSpan.hpp"
#include "VimViewport.hpp"

#include <algorithm>
#include <expected>
#include <optional>
#include <span>

namespace nenenib::application
{
// Vim の問いの往復で、新しい問いかを確かめる純関数（ADR 0129・ADR 0133 の追補 2026-10-18）。
// 往復は毎回「新しい行の行数」「新しい (行, 段) の画面行」「表示領域 1 回」のどれかを足すので、
// 同じ問いの立て直しと表示領域の二重要求を破れとすれば、往復は行数 + 画面行の総数 + 1 で尽きる。
// 破れは作り直しでは直らないので inconsistent（unavailable にしない）。

// 表示領域はまだ渡していないか。
[[nodiscard]] constexpr std::expected<void, TextLayoutFailure>
vim_viewport_unasked(const std::optional<core::VimViewport> &held) noexcept
{
    if (held.has_value())
    {
        return std::unexpected(TextLayoutFailure::inconsistent);
    }
    return {};
}

// 行 line の画面行数はまだ答えていないか。
[[nodiscard]] constexpr std::expected<void, TextLayoutFailure>
vim_rows_unasked(std::span<const core::VimLineRows> rows, core::LineNumber line) noexcept
{
    if (std::ranges::any_of(rows,
                            [line](const core::VimLineRows &value) { return value.line == line; }))
    {
        return std::unexpected(TextLayoutFailure::inconsistent);
    }
    return {};
}

// 同じ (行, 段) の画面行はまだ答えていないか。
[[nodiscard]] constexpr std::expected<void, TextLayoutFailure>
vim_span_unanswered(std::span<const core::VimRowSpan> spans, const core::VimRowSpan &span) noexcept
{
    if (std::ranges::any_of(spans, [&span](const core::VimRowSpan &value)
                            { return value.line == span.line && value.row == span.row; }))
    {
        return std::unexpected(TextLayoutFailure::inconsistent);
    }
    return {};
}
} // namespace nenenib::application
