#include "BodyTextLayouts.hpp"

#include "BodyGlyphCollector.hpp"
#include "DevicePixels.hpp"
#include "FontFallbackCache.hpp"
#include "LineUtf16Evaluation.hpp"
#include "TextLayoutPoint.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr auto unavailable = application::TextLayoutFailure::unavailable;
constexpr float tab_stop_spaces = 8.0F;
constexpr float spacing_probe_width = 1000.0F;
// 升の幅と cluster の送りの差がこれより小さければ揃え直さない（物理画素）。
constexpr float cell_tolerance = 0.01F;
constexpr std::array<const wchar_t *, 2> grid_fonts{L"Cascadia Mono", L"Consolas"};
constexpr wchar_t low_surrogate_first = 0xDC00;
constexpr wchar_t low_surrogate_last = 0xDFFF;

// UTF-16 の [from, from + length) にある code point の数（下位サロゲートを数えない）。
[[nodiscard]] std::size_t code_points(const std::wstring &wide, UINT32 from, UINT32 length)
{
    std::size_t count = 0;
    for (auto unit = from; unit < from + length && unit < wide.size(); ++unit)
    {
        const auto value = wide.at(unit);
        count += value >= low_surrogate_first && value <= low_surrogate_last ? 0U : 1U;
    }
    return count;
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
    // 字幅（average_advance）は書式から測る値で、書式の鍵ではない（ADR 0139）。
    const auto &geometry = style_.has_value() ? style_.value().geometry : request.geometry;
    return style_.has_value() && style_.value().family == request.settings.font_family.text() &&
           style_.value().points == request.settings.font_size.points() &&
           geometry.width == request.geometry.width &&
           geometry.line_height == request.geometry.line_height &&
           geometry.dpi == request.geometry.dpi &&
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
    pending_.reset();
    cells_ = {};
    grid_format_.Reset();
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

bool BodyTextLayouts::exists(const std::wstring &name) const
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(factory_->GetSystemFontCollection(&collection)) &&
           SUCCEEDED(collection->FindFamilyName(name.c_str(), &index, &exists)) && exists != FALSE;
}

std::wstring BodyTextLayouts::family(std::string_view requested) const
{
    auto wide = core::to_utf16(requested).value();
    return exists(wide) ? wide : std::wstring{L"Consolas"};
}

std::wstring BodyTextLayouts::grid_family(std::string_view requested) const
{
    for (const auto *name : grid_fonts)
    {
        if (exists(name))
        {
            return name;
        }
    }
    return family(requested);
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
    const auto finished = finish_format(format_.Get(), request);
    if (FAILED(finished))
    {
        return finished;
    }
    return format_->SetWordWrapping(request.geometry.width > 0 ? wrapping(request.settings.wrapping)
                                                               : DWRITE_WORD_WRAPPING_NO_WRAP);
}

// 格子は画面行ごとに 1 つの layout で、折り返しは application が升で決める（NO_WRAP）。
HRESULT BodyTextLayouts::make_grid_format(const application::TextLayoutRequest &request)
{
    const auto face = grid_family(request.settings.font_family.text());
    const float size = core::font_size_dips(request.settings.font_size) *
                       static_cast<float>(request.geometry.dpi) /
                       static_cast<float>(core::reference_dpi);
    TextFormat made;
    auto result = factory_->CreateTextFormat(face.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                             DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                             size, L"", &made);
    if (SUCCEEDED(result))
    {
        result = finish_format(made.Get(), request);
    }
    if (SUCCEEDED(result))
    {
        result = made->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    if (SUCCEEDED(result))
    {
        grid_format_ = std::move(made);
    }
    return result;
}

HRESULT BodyTextLayouts::finish_format(IDWriteTextFormat *format,
                                       const application::TextLayoutRequest &request)
{
    attach_fallback(format);
    return set_spacing(format, request);
}

IDWriteTextFormat *BodyTextLayouts::format_of(const application::TextLayoutRequest &request)
{
    switch (request.form)
    {
    case core::DisplayForm::text:
        return format_.Get();
    case core::DisplayForm::grid:
        if (!grid_format_ && FAILED(make_grid_format(request)))
        {
            return nullptr;
        }
        return grid_format_.Get();
    }
    std::unreachable();
}

void BodyTextLayouts::attach_fallback(IDWriteTextFormat *format)
{
    Microsoft::WRL::ComPtr<IDWriteFontFallback> system;
    Microsoft::WRL::ComPtr<IDWriteTextFormat2> version;
    const Microsoft::WRL::ComPtr<IDWriteTextFormat> held(format);
    if (FAILED(factory_->GetSystemFontFallback(&system)) || FAILED(held.As(&version)))
    {
        return;
    }
    // ADR 0077: 保持を付けられなければOSの同じ字体選択で描ける。
    static_cast<void>(
        version->SetFontFallback(Microsoft::WRL::Make<FontFallbackCache>(system).Get()));
}

HRESULT BodyTextLayouts::set_spacing(IDWriteTextFormat *format,
                                     const application::TextLayoutRequest &request)
{
    TextLayout space;
    const float height = static_cast<float>(std::max(request.geometry.line_height, 1));
    const auto made =
        factory_->CreateTextLayout(L" ", 1, format, spacing_probe_width, height, &space);
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
    const auto tab = format->SetIncrementalTabStop(
        std::max(text.widthIncludingTrailingWhitespace, 1.0F) * tab_stop_spaces);
    if (FAILED(tab))
    {
        return tab;
    }
    const float baseline = (height - line.height) / 2.0F + line.baseline;
    return format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, height, baseline);
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
    auto *format = format_of(request);
    TextLayout made;
    if (format == nullptr ||
        FAILED(factory_->CreateTextLayout(
            wide.value().c_str(), static_cast<UINT32>(wide.value().size()), format,
            static_cast<float>(area.right), static_cast<float>(area.bottom), &made)) ||
        FAILED(align_cells(made.Get(), wide.value(), request)))
    {
        return std::unexpected(unavailable);
    }
    // 巨大な行の窓は設定の折り返しにかかわらず折り返さない（ADR 0139 の決定 2）。
    if (request.window.active && FAILED(made->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
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
    entry.form = request.form;
    entry.unwrapped = request.window.active;
    return entry;
}

// cluster ごとに、送りを cells の升の幅（幅 2 は 2 升・幅 0 は前の cluster に含まれる）へ合わせる。
// 前の空き（leading）は 0 なので字形は升の左端に立ち、後ろの空き（trailing）で差を埋める。
HRESULT BodyTextLayouts::align_cells(IDWriteTextLayout *layout, const std::wstring &wide,
                                     const application::TextLayoutRequest &request)
{
    if (request.form != core::DisplayForm::grid || request.cells.empty())
    {
        return S_OK;
    }
    const auto measured = cell(request);
    Microsoft::WRL::ComPtr<IDWriteTextLayout1> spaced;
    UINT32 count = 0;
    static_cast<void>(layout->GetClusterMetrics(nullptr, 0, &count));
    std::vector<DWRITE_CLUSTER_METRICS> clusters(count);
    if (!measured || FAILED(Microsoft::WRL::ComPtr<IDWriteTextLayout>(layout).As(&spaced)) ||
        FAILED(layout->GetClusterMetrics(clusters.data(), count, &count)))
    {
        return E_FAIL;
    }
    const auto last = request.cells.size() - 1;
    UINT32 position = 0;
    std::size_t point = 0;
    for (const auto &cluster : clusters)
    {
        const auto next = std::min(point + code_points(wide, position, cluster.length), last);
        const auto target =
            static_cast<float>(request.cells[next] - request.cells[std::min(point, last)]) *
            measured.value().width;
        if (std::abs(target - cluster.width) > cell_tolerance &&
            FAILED(spaced->SetCharacterSpacing(0, target - cluster.width, 0,
                                               DWRITE_TEXT_RANGE{position, cluster.length})))
        {
            return E_FAIL;
        }
        position += cluster.length;
        point = next;
    }
    return S_OK;
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
    {
        return entry.layout && entry.form == request.form &&
               entry.unwrapped == request.window.active && entry.text == request.text;
    };
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
    if (pending_.has_value() && matches(pending_.value()))
    {
        auto entry = std::move(pending_).value();
        pending_.reset();
        return retain(std::move(entry));
    }
    auto made = make_layout(request);
    if (!made)
    {
        return std::unexpected(made.error());
    }
    return retain(std::move(made).value());
}

BodyTextLayouts::TextLayout BodyTextLayouts::retain(BodyTextLayout entry)
{
    if (collecting_)
    {
        current_.push_back(std::move(entry));
        return current_.back().layout;
    }
    pending_ = std::move(entry);
    return pending_.value().layout;
}

BodyTextLayout *BodyTextLayouts::find(IDWriteTextLayout *layout)
{
    const auto found = std::ranges::find_if(current_, [layout](const BodyTextLayout &entry)
                                            { return entry.layout.Get() == layout; });
    if (found != current_.end())
    {
        return &*found;
    }
    if (pending_.has_value() && pending_.value().layout.Get() == layout)
    {
        return &pending_.value();
    }
    return nullptr;
}

std::expected<std::size_t, application::TextLayoutFailure>
BodyTextLayouts::rows(const application::TextLayoutRequest &request)
{
    const auto layout = get(request);
    if (!layout)
    {
        return std::unexpected(layout.error());
    }
    const auto *entry = find(layout.value().Get());
    if (entry == nullptr)
    {
        std::unreachable();
    }
    return entry->rows;
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
    const auto point = text_layout_point(layout.value().Get(), units, caret.affinity);
    if (!point)
    {
        return std::unexpected(unavailable);
    }
    const auto height = static_cast<float>(std::max(request.geometry.line_height, 1));
    return core::TextLocation{static_cast<std::size_t>(std::max(point.value().y, 0.0F) / height),
                              point.value().x};
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

std::expected<application::TextCell, application::TextLayoutFailure>
BodyTextLayouts::cell(const application::TextLayoutRequest &request)
{
    const auto ready = prepare(request);
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    auto &held = cells_.at(static_cast<std::size_t>(request.form));
    if (held.has_value())
    {
        return held.value();
    }
    // 保持には入れない（本文の文字組みの鍵は表示文字列のまま・ADR 0114）。
    const auto made = make_layout(
        application::TextLayoutRequest{"0", request.settings, request.geometry, request.form});
    if (!made)
    {
        return std::unexpected(made.error());
    }
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(made.value().layout->GetMetrics(&metrics)) ||
        metrics.widthIncludingTrailingWhitespace <= 0 || metrics.height <= 0)
    {
        return std::unexpected(unavailable);
    }
    held = application::TextCell{metrics.widthIncludingTrailingWhitespace, metrics.height};
    return held.value();
}

const BodyTextLayout *BodyTextLayouts::glyphs_of(IDWriteTextLayout *layout, float left)
{
    auto *found = find(layout);
    if (found == nullptr || !style_.has_value())
    {
        return nullptr;
    }
    if (!found->glyphs_tried || found->glyph_left != left)
    {
        const auto width = static_cast<float>(core::width_of(found->area));
        auto collector = Microsoft::WRL::Make<BodyGlyphCollector>(width, left);
        auto taken = collector && SUCCEEDED(layout->Draw(nullptr, collector.Get(), 0, 0))
                         ? collector->take()
                         : std::nullopt;
        found->glyphs_ready = taken.has_value();
        found->glyphs = std::move(taken).value_or(std::vector<BodyGlyphRun>{});
        found->glyphs_tried = true;
        found->glyph_left = left;
    }
    return found;
}

void BodyTextLayouts::begin_frame()
{
    previous_.swap(current_);
    current_.clear();
    collecting_ = true;
}

void BodyTextLayouts::end_frame()
{
    previous_.clear();
    pending_.reset();
    collecting_ = false;
}
} // namespace nenenib::ui::win32
