#include "LinkCardFormats.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
// 書式の大きさ（DIP）と太さ。2 つ目（target）だけ等幅の字体。
using FormatShape = std::pair<float, DWRITE_FONT_WEIGHT>;
constexpr std::array<FormatShape, 5> shapes{{
    {13.0F, DWRITE_FONT_WEIGHT_BOLD},
    {13.0F, DWRITE_FONT_WEIGHT_NORMAL},
    {12.5F, DWRITE_FONT_WEIGHT_NORMAL},
    {11.5F, DWRITE_FONT_WEIGHT_NORMAL},
    {13.0F, DWRITE_FONT_WEIGHT_SEMI_BOLD},
}};
constexpr std::size_t mono_index = 1;
} // namespace

bool LinkCardFormats::made(IDWriteFactory &dwrite,
                           std::pair<const wchar_t *, const wchar_t *> faces, float scale,
                           const wchar_t *tag)
{
    if (ready_)
    {
        return true;
    }
    for (std::size_t index = 0; index < shapes.size(); ++index)
    {
        auto &format = formats_.at(index);
        const auto &[size, weight] = shapes.at(index);
        if (FAILED(dwrite.CreateTextFormat(index == mono_index ? faces.second : faces.first,
                                           nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL, size * scale, tag,
                                           format.ReleaseAndGetAddressOf())))
        {
            reset();
            return false;
        }
    }
    ready_ = true;
    return true;
}

void LinkCardFormats::reset() noexcept
{
    for (auto &format : formats_)
    {
        format.Reset();
    }
    ready_ = false;
}

IDWriteTextFormat *LinkCardFormats::title() const noexcept
{
    return formats_.at(0).Get();
}

IDWriteTextFormat *LinkCardFormats::target() const noexcept
{
    return formats_.at(1).Get();
}

IDWriteTextFormat *LinkCardFormats::way() const noexcept
{
    return formats_.at(2).Get();
}

IDWriteTextFormat *LinkCardFormats::hint() const noexcept
{
    return formats_.at(3).Get();
}

IDWriteTextFormat *LinkCardFormats::button() const noexcept
{
    return formats_.at(4).Get();
}
} // namespace nenenib::ui::win32
