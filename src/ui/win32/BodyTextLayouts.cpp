#include "BodyTextLayouts.hpp"

#include "BodyGlyphCollector.hpp"
#include "DevicePixels.hpp"
#include "FontFallbackCache.hpp"
#include "LineUtf16Evaluation.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr auto unavailable = application::TextLayoutFailure::unavailable;
constexpr float tab_stop_spaces = 8.0F;
constexpr float spacing_probe_width = 1000.0F;

[[nodiscard]] bool trailing(core::CaretAffinity affinity) noexcept
{
    switch (affinity)
    {
    case core::CaretAffinity::leading:
        return false;
    case core::CaretAffinity::trailing:
        return true;
    }
    std::unreachable();
}

[[nodiscard]] DWRITE_WORD_WRAPPING wrapping(core::LineWrapping value) noexcept
{
    switch (value)
    {
    case core::LineWrapping::wrap:
        return DWRITE_WORD_WRAPPING_CHARACTER;
    case core::LineWrapping::no_wrap:
        return DWRITE_WORD_WRAPPING_NO_WRAP;
    }
    std::unreachable();
}
} // namespace

bool BodyTextLayouts::matches_style(const application::TextLayoutRequest &request) const
{
    return style_.has_value() && style_.value().family == request.settings.font_family.text() &&
           style_.value().points == request.settings.font_size.points() &&
           style_.value().geometry == request.geometry &&
           style_.value().wrapping == request.settings.wrapping;
}

std::expected<void, application::TextLayoutFailure>
BodyTextLayouts::prepare(const application::TextLayoutRequest &request)
{
    if (matches_style(request))
    {
        return {};
    }
    current_.clear();
    previous_.clear();
    style_.reset();
    if (!factory_)
    {
        Microsoft::WRL::ComPtr<IUnknown> created;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7),
                                       created.GetAddressOf())) ||
            FAILED(created.As(&factory_)))
        {
            return std::unexpected(unavailable);
        }
    }
    if (FAILED(make_format(request)))
    {
        return std::unexpected(unavailable);
    }
    style_ = BodyTextStyle{std::string(request.settings.font_family.text()),
                           request.settings.font_size.points(), request.geometry,
                           request.settings.wrapping};
    return {};
}

std::wstring BodyTextLayouts::family(std::string_view requested) const
{
    const auto wide = core::to_utf16(requested).value();
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (SUCCEEDED(factory_->GetSystemFontCollection(&collection)) &&
        SUCCEEDED(collection->FindFamilyName(wide.c_str(), &index, &exists)) && exists != FALSE)
    {
        return wide;
    }
    return L"Consolas";
}

HRESULT BodyTextLayouts::make_format(const application::TextLayoutRequest &request)
{
    format_.Reset();
    const auto face = family(request.settings.font_family.text());
    const float size = core::font_size_dips(request.settings.font_size) *
                       static_cast<float>(request.geometry.dpi) /
                       static_cast<float>(core::reference_dpi);
    const auto made = factory_->CreateTextFormat(face.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                                 DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, size, L"", &format_);
    if (FAILED(made))
    {
        return made;
    }
    attach_fallback();
    const auto spacing = set_spacing(request);
    if (FAILED(spacing))
    {
        return spacing;
    }
    return format_->SetWordWrapping(request.geometry.width > 0 ? wrapping(request.settings.wrapping)
                                                               : DWRITE_WORD_WRAPPING_NO_WRAP);
}

void BodyTextLayouts::attach_fallback()
{
    Microsoft::WRL::ComPtr<IDWriteFontFallback> system;
    Microsoft::WRL::ComPtr<IDWriteTextFormat2> version;
    if (FAILED(factory_->GetSystemFontFallback(&system)) || FAILED(format_.As(&version)))
    {
        return;
    }
    // ADR 0077: 保持を付けられなければOSの同じ字体選択で描ける。
    static_cast<void>(
        version->SetFontFallback(Microsoft::WRL::Make<FontFallbackCache>(system).Get()));
}

HRESULT BodyTextLayouts::set_spacing(const application::TextLayoutRequest &request)
{
    TextLayout space;
    const float height = static_cast<float>(std::max(request.geometry.line_height, 1));
    const auto made =
        factory_->CreateTextLayout(L" ", 1, format_.Get(), spacing_probe_width, height, &space);
    if (FAILED(made))
    {
        return made;
    }
    DWRITE_LINE_METRICS line{};
    DWRITE_TEXT_METRICS text{};
    UINT32 count = 0;
    if (FAILED(space->GetLineMetrics(&line, 1, &count)) || FAILED(space->GetMetrics(&text)))
    {
        return E_FAIL;
    }
    const auto tab = format_->SetIncrementalTabStop(
        std::max(text.widthIncludingTrailingWhitespace, 1.0F) * tab_stop_spaces);
    if (FAILED(tab))
    {
        return tab;
    }
    const float baseline = (height - line.height) / 2.0F + line.baseline;
    return format_->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, height, baseline);
}

std::expected<BodyTextLayout, application::TextLayoutFailure>
BodyTextLayouts::make_layout(const application::TextLayoutRequest &request)
{
    const auto wide = core::to_utf16(request.text);
    if (!wide)
    {
        return std::unexpected(unavailable);
    }
    const core::LayoutRect area{0, 0, std::max(request.geometry.width, 1),
                                std::max(request.geometry.line_height, 1)};
    TextLayout made;
    if (FAILED(factory_->CreateTextLayout(
            wide.value().c_str(), static_cast<UINT32>(wide.value().size()), format_.Get(),
            static_cast<float>(area.right), static_cast<float>(area.bottom), &made)))
    {
        return std::unexpected(unavailable);
    }
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(made->GetMetrics(&metrics)))
    {
        return std::unexpected(unavailable);
    }
    BodyTextLayout entry{std::string(request.text), area, std::move(made)};
    entry.rows = std::max<std::size_t>(metrics.lineCount, 1);
    return entry;
}

std::expected<BodyTextLayouts::TextLayout, application::TextLayoutFailure>
BodyTextLayouts::get(const application::TextLayoutRequest &request)
{
    const auto ready = prepare(request);
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    const auto matches = [&request](const BodyTextLayout &entry)
    { return entry.layout && entry.text == request.text; };
    const auto current = std::ranges::find_if(current_, matches);
    if (current != current_.end())
    {
        return current->layout;
    }
    const auto previous = std::ranges::find_if(previous_, matches);
    if (previous != previous_.end())
    {
        current_.push_back(std::move(*previous));
        return current_.back().layout;
    }
    auto made = make_layout(request);
    if (!made)
    {
        return std::unexpected(made.error());
    }
    current_.push_back(std::move(made).value());
    return current_.back().layout;
}

std::expected<std::size_t, application::TextLayoutFailure>
BodyTextLayouts::rows(const application::TextLayoutRequest &request)
{
    const auto layout = get(request);
    if (!layout)
    {
        return std::unexpected(layout.error());
    }
    const auto found = std::ranges::find_if(current_, [&layout](const BodyTextLayout &entry)
                                            { return entry.layout == layout.value(); });
    return found->rows;
}

std::expected<core::TextLocation, application::TextLayoutFailure>
BodyTextLayouts::locate(const application::TextLayoutRequest &request, core::DisplayCaret caret)
{
    const auto layout = get(request);
    if (!layout)
    {
        return std::unexpected(layout.error());
    }
    LineUtf16Evaluation positions(request.text);
    const auto units = positions.units_of(caret.column);
    const bool after = trailing(caret.affinity) && units > 0;
    DWRITE_HIT_TEST_METRICS metrics{};
    float x = 0;
    float y = 0;
    if (FAILED(layout.value()->HitTestTextPosition(units - (after ? 1U : 0U), after ? TRUE : FALSE,
                                                   &x, &y, &metrics)))
    {
        return std::unexpected(unavailable);
    }
    const auto height = static_cast<float>(std::max(request.geometry.line_height, 1));
    return core::TextLocation{static_cast<std::size_t>(std::max(y, 0.0F) / height), x};
}

std::expected<core::DisplayCaret, application::TextLayoutFailure>
BodyTextLayouts::hit(const application::TextLayoutRequest &request, core::TextLocation point)
{
    const auto layout = get(request);
    if (!layout)
    {
        return std::unexpected(layout.error());
    }
    BOOL after = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    const float y = (static_cast<float>(point.row) + 0.5F) *
                    static_cast<float>(std::max(request.geometry.line_height, 1));
    if (FAILED(layout.value()->HitTestPoint(point.x, y, &after, &inside, &metrics)))
    {
        return std::unexpected(unavailable);
    }
    const auto units = metrics.textPosition + (after != FALSE ? metrics.length : 0U);
    LineUtf16Evaluation positions(request.text);
    return core::DisplayCaret{positions.column_of(units), after != FALSE
                                                              ? core::CaretAffinity::trailing
                                                              : core::CaretAffinity::leading};
}

const BodyTextLayout *BodyTextLayouts::glyphs_of(IDWriteTextLayout *layout)
{
    const auto found = std::ranges::find_if(current_, [layout](const BodyTextLayout &entry)
                                            { return entry.layout.Get() == layout; });
    if (found == current_.end())
    {
        return nullptr;
    }
    if (!found->glyphs_tried)
    {
        auto collector = Microsoft::WRL::Make<BodyGlyphCollector>(
            static_cast<float>(core::width_of(found->area)));
        auto taken = collector && SUCCEEDED(layout->Draw(nullptr, collector.Get(), 0, 0))
                         ? collector->take()
                         : std::nullopt;
        found->glyphs_ready = taken.has_value();
        found->glyphs = std::move(taken).value_or(std::vector<BodyGlyphRun>{});
        found->glyphs_tried = true;
    }
    return &*found;
}

void BodyTextLayouts::begin_frame()
{
    previous_.swap(current_);
    current_.clear();
}

void BodyTextLayouts::end_frame()
{
    previous_.clear();
}
} // namespace nenenib::ui::win32
