#include "PreviewBrushes.hpp"

#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

// 役割 → 色の唯一の表（採用案 D62 の §3）。
[[nodiscard]] core::RgbColor color_of(PreviewInk ink, const core::Theme &theme) noexcept
{
    switch (ink)
    {
    case PreviewInk::background:
        return theme.body.background;
    case PreviewInk::foreground:
        return theme.body.foreground;
    case PreviewInk::keyword:
        return theme.body.keyword;
    case PreviewInk::string:
        return theme.body.string;
    case PreviewInk::function:
        return theme.body.function;
    case PreviewInk::comment:
        return theme.body.comment;
    case PreviewInk::operators:
        return theme.body.operators;
    case PreviewInk::panel:
        return theme.ui.panel;
    }
    std::unreachable();
}
} // namespace

HRESULT PreviewBrushes::prepare(ID2D1RenderTarget &target, const core::Theme &theme)
{
    if (target_ == &target && theme_ == theme.name && brushes_.front() != nullptr)
    {
        return S_OK;
    }
    target_ = nullptr;
    for (std::size_t index = 0; index < ink_count; ++index)
    {
        brushes_.at(index).Reset();
        const auto made = target.CreateSolidColorBrush(
            to_color(color_of(static_cast<PreviewInk>(index), theme)), &brushes_.at(index));
        if (FAILED(made))
        {
            return made;
        }
    }
    target_ = &target;
    theme_ = theme.name;
    return S_OK;
}

ID2D1SolidColorBrush *PreviewBrushes::of(PreviewInk ink) const
{
    return brushes_.at(static_cast<std::size_t>(ink)).Get();
}
} // namespace nenenib::ui::win32
