#pragma once

#include "DevicePixels.hpp"
#include "FontSize.hpp"

#include <cstdint>

namespace nenenib::ui::win32
{
// 行番号の欄の文字の寸法（docs/design/2026-09-15-look.md 第 2 節）。画面の renderer と印刷
// （ADR 0177 の決定 5）が同じ値で番号を描く。欄の幅は core の body_layout_in の 1 本。
inline constexpr float gutter_text_dips = 12.0F;
inline constexpr std::int32_t gutter_padding_dips = 16;

// 番号の右（左寄せの番号は左にも）の余白の物理画素。字の大きさの倍率に追従する。
[[nodiscard]] inline std::int32_t gutter_padding(core::FontSize size, std::uint32_t dpi) noexcept
{
    return core::to_pixels(
        static_cast<std::int32_t>(
            static_cast<float>(gutter_padding_dips) * core::font_size_ratio(size) + 0.5F),
        dpi);
}
} // namespace nenenib::ui::win32
