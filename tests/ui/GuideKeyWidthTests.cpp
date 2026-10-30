// #1122: 空の文書の案内の鍵の箱の幅は、鍵の表示名を描画と同じ書式の layout で測った幅から決める
// （ADR 0078 の追補）。箱で切った画素と切らない画素が同じなら、字は箱に収まっている。
// 本物の DirectWrite とメモリの bitmap で描く（窓・device は作らない）。字体に結果が依るので、
// Cascadia Code も Consolas も無い機械では測らない。
#include "BodyLayout.hpp"
#include "DevicePixels.hpp"
#include "DirectWriteFactory.hpp"
#include "EditMode.hpp"
#include "GuideKeyWidth.hpp"
#include "GuideRequest.hpp"
#include "OperationGuide.hpp"
#include "OperationGuideLayout.hpp"
#include "SoftwareTarget.hpp"
#include "StatusBarLayout.hpp"
#include "Utf16.hpp"
#include "WindowChecks.hpp"

#include <algorithm>
#include <string>
#include <variant>

namespace nenenib::tests::ui
{
namespace
{
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Layout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
using nenenib::ui::win32::guide_key_width;
// Direct2DRenderer の guide_key_padding_dips と書式（12.5 DIP・NO_WRAP・縦は中央）と同じ値。
constexpr float padding = 9.0F;
constexpr std::uint32_t dpi = 96;

[[nodiscard]] Format key_format(IDWriteFactory2 &factory, const wchar_t *family)
{
    Format format;
    if (FAILED(factory.CreateTextFormat(family, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.5F,
                                        L"", &format)))
    {
        return nullptr;
    }
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return format;
}

// 本文の案内の鍵の列（core の配置）。
[[nodiscard]] core::LayoutRect key_column(std::int32_t key_width)
{
    const auto body = core::body_layout(960, 560, dpi, core::default_font_size());
    const auto status = core::status_bar_layout(960, 560, dpi);
    const auto guide = core::operation_guide_layout(
        core::GuideRequest{core::GuideContext::untouched_untitled, key_width}, body, status, dpi);
    const auto *layout = std::get_if<core::BodyGuideLayout>(&guide);
    expect(layout != nullptr, "a 960 x 560 DIP window shows the body guide");
    return layout == nullptr ? core::LayoutRect{} : layout->rows.front().key;
}

// draw_keycap と同じく欄の幅の layout を作る。
[[nodiscard]] Layout column_layout(IDWriteFactory2 &factory, IDWriteTextFormat *format,
                                   const std::string &key, const core::LayoutRect &column)
{
    const std::wstring wide = core::to_utf16(key).value_or(std::wstring{});
    Layout layout;
    const auto made =
        factory.CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()), format,
                                 static_cast<float>(core::width_of(column)),
                                 static_cast<float>(core::height_of(column)), &layout);
    expect(SUCCEEDED(made), "the key layout is created");
    return SUCCEEDED(made) ? layout : nullptr;
}

// 右端を欄の右端に揃えた箱で切って描いた画素が、切らずに描いた画素と同じか。欄は描画先の
// 中ほどへ移す（幅と高さは core の配置のまま）。
[[nodiscard]] bool fits(SoftwareTarget &target, const Layout &layout,
                        const core::LayoutRect &column)
{
    DWRITE_TEXT_METRICS metrics{};
    if (!layout || FAILED(layout->GetMetrics(&metrics)))
    {
        return false;
    }
    const float right = 600.0F;
    const float width =
        std::min(metrics.width + (padding * 2.0F), static_cast<float>(core::width_of(column)));
    const D2D1_POINT_2F origin{right - width + padding, 30.0F};
    const auto boxed =
        target.clipped_layout(layout.Get(), origin, D2D1_RECT_F{right - width, 0.0F, right, 80.0F});
    const auto open =
        target.clipped_layout(layout.Get(), origin,
                              D2D1_RECT_F{0.0F, 0.0F, static_cast<float>(SoftwareTarget::width),
                                          static_cast<float>(SoftwareTarget::height)});
    expect(std::ranges::any_of(open, [](BYTE value) { return value != 0; }),
           "the key draws some pixels");
    return !open.empty() && boxed == open;
}

void verify_with(SoftwareTarget &target, IDWriteFactory2 &factory, IDWriteTextFormat *format)
{
    const auto fit = [&](const std::string &key, const core::LayoutRect &column)
    { return fits(target, column_layout(factory, format, key, column), column); };
    const auto vim =
        core::operation_guide(core::GuideContext::untouched_untitled, core::EditMode::vim);
    const auto ordinary =
        core::operation_guide(core::GuideContext::untouched_untitled, core::EditMode::ordinary);
    expect(std::ranges::any_of(vim.entries, [](const core::GuideEntry &entry)
                               { return entry.key == "Ctrl+Shift+O"; }),
           "the Vim guide shows Ctrl+Shift+O (#1037)");
    const auto vim_width = guide_key_width(factory, *format, vim.entries, padding);
    const auto vim_column = key_column(vim_width);
    for (const auto &entry : vim.entries)
    {
        expect(fit(entry.key, vim_column),
               "every Vim guide key fits inside its measured box (#1122)");
    }
    const auto fixed = key_column(0);
    expect(!fit("Ctrl+Shift+O", fixed),
           "the adopted 64 DIP column alone cuts Ctrl+Shift+O (the reported defect)");
    const auto ordinary_width = guide_key_width(factory, *format, ordinary.entries, padding);
    expect(ordinary_width > 0 && ordinary_width <= core::to_pixels(64, dpi),
           "the ordinary guide keys are measured within the adopted 64 DIP");
    expect(key_column(ordinary_width) == fixed,
           "the ordinary guide keeps the adopted layout unchanged");
    for (const auto &entry : ordinary.entries)
    {
        expect(fit(entry.key, fixed), "every ordinary guide key fits inside the adopted box");
    }
}
} // namespace

void verify_guide_key_width()
{
    const auto factory = directwrite_factory();
    SoftwareTarget target;
    if (!factory || !target.ready())
    {
        unmeasured("guide key width: DirectWrite or the software target is unavailable");
        return;
    }
    const wchar_t *family = installed(*factory.Get(), L"Cascadia Code") ? L"Cascadia Code"
                            : installed(*factory.Get(), L"Consolas")    ? L"Consolas"
                                                                        : nullptr;
    if (family == nullptr)
    {
        unmeasured("guide key width: neither Cascadia Code nor Consolas is installed");
        return;
    }
    const auto format = key_format(*factory.Get(), family);
    expect(format != nullptr, "the guide key format is created");
    if (format)
    {
        verify_with(target, *factory.Get(), format.Get());
    }
}
} // namespace nenenib::tests::ui
