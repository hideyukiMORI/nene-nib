#pragma once

#include "CaretAffinity.hpp"
#include "TextLayoutFailure.hpp"

#include <dwrite.h>
#include <expected>

namespace nenenib::ui::win32
{
struct TextLayoutPoint
{
    float x;
    float y;
};

[[nodiscard]] std::expected<TextLayoutPoint, application::TextLayoutFailure>
text_layout_point(IDWriteTextLayout *layout, UINT32 units, core::CaretAffinity affinity);
} // namespace nenenib::ui::win32
