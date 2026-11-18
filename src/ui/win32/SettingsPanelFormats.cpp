#include "SettingsPanelFormats.hpp"

#include <cstddef>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
// 書式の大きさ（DIP）と太さ。最後の 1 つ（mono）だけ等幅の字体。
using FormatShape = std::pair<float, DWRITE_FONT_WEIGHT>;
constexpr std::array<FormatShape, 7> shapes{{
    {17.0F, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {13.0F, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {11.5F, DWRITE_FONT_WEIGHT_BOLD},
    {11.5F, DWRITE_FONT_WEIGHT_NORMAL},
    {12.0F, DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {12.5F, DWRITE_FONT_WEIGHT_NORMAL},
    {11.0F, DWRITE_FONT_WEIGHT_NORMAL},
}};
constexpr std::size_t mono_index = 6;
} // namespace

bool SettingsPanelFormats::made(IDWriteFactory &dwrite,
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
                                           format.ReleaseAndGetAddressOf())) ||
            FAILED(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)) ||
            FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
        {
            reset();
            return false;
        }
    }
    ready_ = true;
    return true;
}

void SettingsPanelFormats::reset() noexcept
{
    for (auto &format : formats_)
    {
        format.Reset();
    }
    ready_ = false;
}

SettingsPanelText SettingsPanelFormats::text(IDWriteFactory *dwrite, std::uint32_t dpi,
                                             core::DisplayLanguage language) const
{
    return SettingsPanelText{dwrite,
                             formats_.at(0).Get(),
                             formats_.at(1).Get(),
                             formats_.at(2).Get(),
                             formats_.at(3).Get(),
                             formats_.at(4).Get(),
                             formats_.at(5).Get(),
                             formats_.at(6).Get(),
                             dpi,
                             language};
}
} // namespace nenenib::ui::win32
