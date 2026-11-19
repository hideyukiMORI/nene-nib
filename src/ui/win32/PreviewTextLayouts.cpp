#include "PreviewTextLayouts.hpp"

#include "PreviewImageObject.hpp"
#include "PreviewInlineImages.hpp"
#include "PreviewMetrics.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewTableCell.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <dwrite_1.h>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
constexpr auto directwrite = RenderFailure::directwrite;
// 格子と同じ等幅の字体の候補（ADR 0140 の決定 6・D58）。
constexpr std::array<const wchar_t *, 2> code_fonts{L"Cascadia Mono", L"Consolas"};
// layout の高さの上限（物理画素）。高さは GetMetrics から読むので、ここは折り返しに効かない。
constexpr float unbounded_height = 1.0e7F;

[[nodiscard]] bool same_key(const PreviewTextLayout &entry, const core::PreviewBlock &block,
                            float width, const PreviewTextStyle &style)
{
    return entry.kind == block.kind && entry.level == block.level && entry.width == width &&
           entry.text == block.text && entry.spans == block.spans && entry.style == style &&
           entry.colors == block.code_colors;
}

// 列 column の升を行の順に改行でつないだ要素（装飾は升の中から写す・幅だけを測る）。
[[nodiscard]] core::PreviewBlock column_block(std::span<const core::PreviewBlock> rows,
                                              std::size_t column)
{
    core::PreviewBlock joined{.kind = core::PreviewBlockKind::paragraph,
                              .level = 0U,
                              .quote_depth = 0U,
                              .list_depth = 0U,
                              .marker = {},
                              .source_begin = core::Offset{0U},
                              .text = {},
                              .spans = {},
                              .task = std::nullopt,
                              .table = std::nullopt,
                              .alert = std::nullopt,
                              .code_colors = std::nullopt};
    for (const auto &row : rows)
    {
        if (!joined.text.empty())
        {
            joined.text.push_back('\n');
        }
        const auto cell = core::preview_table_cell(row, column);
        const auto at = joined.text.size();
        joined.text.append(cell.text);
        for (const auto &span : cell.spans)
        {
            joined.spans.push_back(core::PreviewSpan{core::Offset{span.begin.value + at},
                                                     core::Offset{span.end.value + at},
                                                     span.style});
        }
    }
    return joined;
}

[[nodiscard]] bool same_boxes(const std::vector<PreviewInlineBox> &held,
                              std::span<const PreviewInlineImage> inlines)
{
    return std::ranges::equal(held, inlines, {}, {}, &PreviewInlineImage::box);
}

[[nodiscard]] std::vector<PreviewInlineBox> boxes_of(std::span<const PreviewInlineImage> inlines)
{
    std::vector<PreviewInlineBox> boxes;
    boxes.reserve(inlines.size());
    std::ranges::transform(inlines, std::back_inserter(boxes), &PreviewInlineImage::box);
    return boxes;
}

[[nodiscard]] HRESULT styled(IDWriteTextLayout &layout, core::PreviewStyle style,
                             DWRITE_TEXT_RANGE range, const std::wstring &code_family)
{
    switch (style)
    {
    case core::PreviewStyle::emphasis:
    case core::PreviewStyle::image:
        return layout.SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
    case core::PreviewStyle::strong:
        return layout.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
    case core::PreviewStyle::code:
        return layout.SetFontFamilyName(code_family.c_str(), range);
    case core::PreviewStyle::link:
        return layout.SetUnderline(TRUE, range);
    case core::PreviewStyle::strikethrough:
        // 線の太さと位置は DirectWrite の字体の値に任せ、色は文字の色のまま（#1840）。
        return layout.SetStrikethrough(TRUE, range);
    case core::PreviewStyle::alert_title:
        return layout.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
    }
    std::unreachable();
}
// 要素の装飾の範囲（spans と同じ順）を layout の UTF-16 の範囲に写す（前から 1 回だけ数える）。
[[nodiscard]] std::vector<DWRITE_TEXT_RANGE> span_ranges(const core::PreviewBlock &block,
                                                         std::string_view shown)
{
    std::vector<PreviewByteRange> bytes;
    bytes.reserve(block.spans.size());
    std::ranges::transform(block.spans, std::back_inserter(bytes), [](const core::PreviewSpan &span)
                           { return PreviewByteRange{span.begin.value, span.end.value}; });
    return preview_unit_ranges(shown, bytes);
}
// 囲みのコードの構文の色の範囲（code_colors と同じ順・#1858）を layout の UTF-16 の範囲に写す。
[[nodiscard]] std::vector<DWRITE_TEXT_RANGE> color_ranges(const core::PreviewBlock &block,
                                                          std::string_view shown)
{
    if (!block.code_colors.has_value())
    {
        return {};
    }
    const auto &colors = block.code_colors.value();
    std::vector<PreviewByteRange> bytes;
    bytes.reserve(colors.size());
    std::ranges::transform(colors, std::back_inserter(bytes),
                           [](const core::PreviewCodeColor &color)
                           { return PreviewByteRange{color.begin.value, color.end.value}; });
    return preview_unit_ranges(shown, bytes);
}
// layout の 1 行目の baseline（GetLineMetrics は欄が足りないと何も書かないので、全部の行を引く）。
[[nodiscard]] std::optional<float> first_baseline(IDWriteTextLayout &layout)
{
    UINT32 count = 0;
    const auto counted = layout.GetLineMetrics(nullptr, 0, &count);
    if ((FAILED(counted) && counted != E_NOT_SUFFICIENT_BUFFER) || count == 0)
    {
        return std::nullopt;
    }
    std::vector<DWRITE_LINE_METRICS> lines(count);
    if (FAILED(layout.GetLineMetrics(lines.data(), count, &count)))
    {
        return std::nullopt;
    }
    return lines.front().baseline;
}
} // namespace

bool PreviewTextLayouts::exists(const std::wstring &name) const
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(factory_->GetSystemFontCollection(&collection)) &&
           SUCCEEDED(collection->FindFamilyName(name.c_str(), &index, &exists)) && exists != FALSE;
}

// 機械にある family の既定の face が等幅か（IDWriteFont1::IsMonospacedFont）。
bool PreviewTextLayouts::monospaced(const std::wstring &name) const
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    Microsoft::WRL::ComPtr<IDWriteFontFamily> found;
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    Microsoft::WRL::ComPtr<IDWriteFont1> version;
    UINT32 index = 0;
    BOOL exists = FALSE;
    return SUCCEEDED(factory_->GetSystemFontCollection(&collection)) &&
           SUCCEEDED(collection->FindFamilyName(name.c_str(), &index, &exists)) &&
           exists != FALSE && SUCCEEDED(collection->GetFontFamily(index, &found)) &&
           SUCCEEDED(found->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL,
                                                 DWRITE_FONT_STYLE_NORMAL, &font)) &&
           SUCCEEDED(font.As(&version)) && version->IsMonospacedFont() != FALSE;
}

// コードは設定の字体が等幅ならそれ、でなければ格子と同じ候補（ADR 0140 の決定 6・D58）。
std::wstring PreviewTextLayouts::code_family(const std::wstring &requested) const
{
    if (monospaced(requested))
    {
        return requested;
    }
    const auto found =
        std::ranges::find_if(code_fonts, [this](const wchar_t *name) { return exists(name); });
    return found == code_fonts.end() ? requested : std::wstring{*found};
}

HRESULT PreviewTextLayouts::make_format(const std::wstring &family, float size, bool wrapped,
                                        TextFormat &format)
{
    format.Reset();
    const auto made = factory_->CreateTextFormat(family.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                                 DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, size, L"", &format);
    if (FAILED(made))
    {
        return made;
    }
    return format->SetWordWrapping(wrapped ? DWRITE_WORD_WRAPPING_WRAP
                                           : DWRITE_WORD_WRAPPING_NO_WRAP);
}

HRESULT PreviewTextLayouts::prepare(const PreviewTextStyle &style)
{
    if (style_.has_value() && style_.value() == style)
    {
        return S_OK;
    }
    style_.reset();
    held_.clear();
    cells_.clear();
    tables_.clear();
    spacings_.clear();
    Microsoft::WRL::ComPtr<IUnknown> created;
    if (!factory_ && (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                                 __uuidof(IDWriteFactory), &created)) ||
                      FAILED(created.As(&factory_))))
    {
        return E_FAIL;
    }
    const auto requested = core::to_utf16(style.family);
    if (!requested)
    {
        return E_INVALIDARG;
    }
    code_family_ = code_family(requested.value());
    const auto size = preview_font_pixels(style);
    auto result = make_format(requested.value(), size, true, body_);
    if (SUCCEEDED(result))
    {
        result = make_format(code_family_, size, false, code_);
    }
    if (SUCCEEDED(result))
    {
        style_ = style;
    }
    return result;
}

// 書式の字の大きさ size での 1 行の自然な高さと baseline（空白 1 つの layout で測る・大きさごとに
// 1 回）。
std::optional<std::pair<float, float>> PreviewTextLayouts::natural(bool mono, float size)
{
    const auto known =
        std::ranges::find_if(spacings_, [mono, size](const Spacing &entry)
                             { return std::get<0>(entry) == mono && std::get<1>(entry) == size; });
    if (known != spacings_.end())
    {
        return std::pair{std::get<2>(*known), std::get<3>(*known)};
    }
    TextLayout probe;
    DWRITE_LINE_METRICS line{};
    UINT32 count = 0;
    if (FAILED(factory_->CreateTextLayout(L" ", 1, mono ? code_.Get() : body_.Get(),
                                          unbounded_height, unbounded_height, &probe)) ||
        FAILED(probe->SetFontSize(size, DWRITE_TEXT_RANGE{0, 1})) ||
        FAILED(probe->GetLineMetrics(&line, 1, &count)))
    {
        return std::nullopt;
    }
    spacings_.emplace_back(mono, size, line.height, line.baseline);
    return std::pair{line.height, line.baseline};
}

// 行高を本文と同じ比の固定にして、字を行の縦の中央に置く（BodyTextLayouts の set_spacing
// と同じ式）。
HRESULT PreviewTextLayouts::space(IDWriteTextLayout &layout, bool mono, float size, float line)
{
    const auto measured = natural(mono, size);
    if (!measured)
    {
        return E_FAIL;
    }
    const auto [height, baseline] = measured.value();
    return layout.SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, line,
                                 (line - height) / 2.0F + baseline);
}

HRESULT PreviewTextLayouts::decorate(IDWriteTextLayout &layout, const core::PreviewBlock &block,
                                     std::span<const DWRITE_TEXT_RANGE> ranges)
{
    for (std::size_t index = 0; index < block.spans.size() && index < ranges.size(); ++index)
    {
        const auto range = ranges[index];
        if (range.length > 0 &&
            FAILED(styled(layout, block.spans.at(index).style, range, code_family_)))
        {
            return E_FAIL;
        }
    }
    return S_OK;
}

HRESULT PreviewTextLayouts::place(IDWriteTextLayout &layout,
                                  std::span<const PreviewInlineImage> inlines, std::uint32_t dpi)
{
    // 固定の行高（space）では箱が上の行に重なるので、箱のある layout は字と箱の自然な高さにする。
    auto result = layout.SetLineSpacing(DWRITE_LINE_SPACING_METHOD_DEFAULT, 0.0F, 0.0F);
    for (const auto &image : inlines)
    {
        if (FAILED(result))
        {
            return result;
        }
        const auto object =
            Microsoft::WRL::Make<PreviewImageObject>(preview_inline_extent(image.box.width, dpi),
                                                     preview_inline_extent(image.box.height, dpi));
        result = object == nullptr
                     ? E_OUTOFMEMORY
                     : layout.SetInlineObject(object.Get(),
                                              DWRITE_TEXT_RANGE{image.box.begin, image.box.length});
    }
    return result;
}

std::expected<PreviewTextLayout, RenderFailure>
PreviewTextLayouts::make(const core::PreviewBlock &block, float width,
                         const PreviewTextStyle &style, std::span<const PreviewInlineImage> inlines)
{
    const auto shown = core::preview_shown_text(block);
    const auto wide = core::to_utf16(shown);
    if (!wide)
    {
        return std::unexpected(directwrite);
    }
    const bool mono = preview_monospaced(block.kind);
    const bool heading = block.kind == core::PreviewBlockKind::heading;
    const auto scale = heading ? preview_heading_scale(block.level) : 1.0F;
    const auto size = preview_font_pixels(style) * scale;
    const auto length = static_cast<UINT32>(wide.value().size());
    const DWRITE_TEXT_RANGE whole{0, std::max<UINT32>(length, 1)};
    auto ranges = span_ranges(block, shown);
    TextLayout made;
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(factory_->CreateTextLayout(wide.value().c_str(), length,
                                          mono ? code_.Get() : body_.Get(), width, unbounded_height,
                                          &made)) ||
        (heading && (FAILED(made->SetFontSize(size, whole)) ||
                     FAILED(made->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, whole)))) ||
        FAILED(inlines.empty() ? space(*made.Get(), mono, size, preview_line_pixels(style) * scale)
                               : place(*made.Get(), inlines, style.dpi)) ||
        FAILED(decorate(*made.Get(), block, ranges)) || FAILED(made->GetMetrics(&metrics)))
    {
        return std::unexpected(directwrite);
    }
    ++created_;
    PreviewTextLayout entry{block.text,
                            block.spans,
                            block.kind,
                            block.level,
                            width,
                            style,
                            boxes_of(inlines),
                            std::move(made),
                            metrics.height,
                            {},
                            {},
                            0,
                            std::move(ranges),
                            0,
                            block.code_colors,
                            color_ranges(block, shown)};
    if (FAILED(locate(entry, block, inlines, style.dpi)))
    {
        return std::unexpected(directwrite);
    }
    return entry;
}

HRESULT PreviewTextLayouts::locate(PreviewTextLayout &entry, const core::PreviewBlock &block,
                                   std::span<const PreviewInlineImage> inlines, std::uint32_t dpi)
{
    for (std::size_t index = 0; index < block.spans.size() && index < entry.ranges.size(); ++index)
    {
        const auto range = entry.ranges.at(index);
        if (block.spans.at(index).style != core::PreviewStyle::code || range.length == 0)
        {
            continue;
        }
        auto runs = hit_runs(*entry.layout.Get(), range);
        if (!runs)
        {
            return E_FAIL;
        }
        entry.code_runs.insert(entry.code_runs.end(), runs.value().begin(), runs.value().end());
    }
    entry.image_origins = preview_inline_origins(*entry.layout.Get(), inlines, dpi);
    return S_OK;
}

HRESULT PreviewTextLayouts::ink(PreviewTextLayout &entry, const core::PreviewBlock &block,
                                const PreviewBrushes &brushes)
{
    for (std::size_t index = 0; index < block.spans.size() && index < entry.ranges.size(); ++index)
    {
        const auto style = block.spans.at(index).style;
        // alert の種類の名前は要素の alert の色（#1848）。
        const auto ink = style == core::PreviewStyle::alert_title && block.alert.has_value()
                             ? std::optional(preview_alert_ink(block.alert.value()))
                             : preview_span_ink(style);
        const auto range = entry.ranges.at(index);
        if (!ink.has_value() || range.length == 0)
        {
            continue;
        }
        ++inked_;
        const auto set = entry.layout->SetDrawingEffect(brushes.of(ink.value()), range);
        if (FAILED(set))
        {
            return set;
        }
    }
    return ink_colors(entry, brushes);
}

HRESULT PreviewTextLayouts::ink_colors(PreviewTextLayout &entry, const PreviewBrushes &brushes)
{
    if (!entry.colors.has_value())
    {
        entry.inked = brushes.generation();
        return S_OK;
    }
    const auto &colors = entry.colors.value();
    for (std::size_t index = 0; index < colors.size() && index < entry.color_ranges.size(); ++index)
    {
        const auto range = entry.color_ranges.at(index);
        if (range.length == 0)
        {
            continue;
        }
        ++inked_;
        const auto set = entry.layout->SetDrawingEffect(
            brushes.of(preview_syntax_ink(colors.at(index).role)), range);
        if (FAILED(set))
        {
            return set;
        }
    }
    entry.inked = brushes.generation();
    return S_OK;
}

std::expected<std::vector<DWRITE_HIT_TEST_METRICS>, RenderFailure>
PreviewTextLayouts::hit_runs(IDWriteTextLayout &layout, DWRITE_TEXT_RANGE range)
{
    if (range.length == 0)
    {
        return std::vector<DWRITE_HIT_TEST_METRICS>{};
    }
    ++hit_tests_;
    UINT32 count = 0;
    const auto counted =
        layout.HitTestTextRange(range.startPosition, range.length, 0.0F, 0.0F, nullptr, 0, &count);
    std::vector<DWRITE_HIT_TEST_METRICS> runs(count);
    if ((FAILED(counted) && counted != E_NOT_SUFFICIENT_BUFFER) ||
        FAILED(layout.HitTestTextRange(range.startPosition, range.length, 0.0F, 0.0F, runs.data(),
                                       count, &count)))
    {
        return std::unexpected(directwrite);
    }
    runs.resize(count);
    return runs;
}

std::vector<PreviewTextLayout>::iterator
PreviewTextLayouts::find(const core::PreviewBlock &block, float width,
                         const PreviewTextStyle &style, std::span<const PreviewInlineImage> inlines)
{
    return std::ranges::find_if(
        pool_of(block), [&](const PreviewTextLayout &entry)
        { return same_key(entry, block, width, style) && same_boxes(entry.inlines, inlines); });
}

std::vector<PreviewTextLayout> &
PreviewTextLayouts::pool_of(const core::PreviewBlock &block) noexcept
{
    return block.kind == core::PreviewBlockKind::table_row ? cells_ : held_;
}

std::expected<std::vector<PreviewTextLayout>::iterator, RenderFailure>
PreviewTextLayouts::hold(const core::PreviewBlock &block, float width,
                         const PreviewTextStyle &style, std::span<const PreviewInlineImage> inlines)
{
    if (FAILED(prepare(style)))
    {
        return std::unexpected(directwrite);
    }
    auto &pool = pool_of(block);
    const auto found = find(block, width, style, inlines);
    // 引いた回を通し番号で覚え、あふれたら最も前に引いたものを捨てる（列を動かさない・
    // 見えている升の数の 2 乗にならない・#1852 のレビュー）。
    if (found != pool.end())
    {
        found->used = ++uses_;
        return found;
    }
    auto made = make(block, width, style, inlines);
    if (!made)
    {
        return std::unexpected(made.error());
    }
    if (pool.size() >= (&pool == &cells_ ? cell_capacity : capacity))
    {
        pool.erase(std::ranges::min_element(pool, {}, &PreviewTextLayout::used));
    }
    made.value().used = ++uses_;
    pool.push_back(std::move(made.value()));
    return std::prev(pool.end());
}

std::expected<PreviewTextLayouts::TextLayout, RenderFailure>
PreviewTextLayouts::get(const core::PreviewBlock &block, float width, const PreviewTextStyle &style,
                        std::span<const PreviewInlineImage> inlines)
{
    const auto held = hold(block, width, style, inlines);
    if (!held)
    {
        return std::unexpected(held.error());
    }
    return held.value()->layout;
}

void PreviewTextLayouts::bind(const PreviewBrushes &brushes)
{
    if (inked_by_ == &brushes && inked_generation_ == brushes.generation())
    {
        return;
    }
    std::erase_if(held_, [](const PreviewTextLayout &entry) { return entry.inked != 0; });
    std::erase_if(cells_, [](const PreviewTextLayout &entry) { return entry.inked != 0; });
    inked_by_ = &brushes;
    inked_generation_ = brushes.generation();
}

std::expected<const PreviewTextLayout *, RenderFailure>
PreviewTextLayouts::paintable(const core::PreviewBlock &block, float width,
                              const PreviewTextStyle &style,
                              std::span<const PreviewInlineImage> inlines)
{
    const auto held = hold(block, width, style, inlines);
    if (!held)
    {
        return std::unexpected(held.error());
    }
    auto &entry = *held.value();
    // brush をまだ作っていない描く先（試験の替え玉）では色を置かない。
    if (inked_by_ != nullptr && inked_generation_ != 0 && entry.inked != inked_generation_ &&
        FAILED(ink(entry, block, *inked_by_)))
    {
        return std::unexpected(directwrite);
    }
    return &entry;
}

std::expected<float, RenderFailure>
PreviewTextLayouts::measure(const core::PreviewBlock &block, float width,
                            const PreviewTextStyle &style,
                            std::span<const PreviewInlineImage> inlines)
{
    const auto held = hold(block, width, style, inlines);
    if (!held)
    {
        return std::unexpected(held.error());
    }
    return held.value()->height;
}

std::expected<float, RenderFailure>
PreviewTextLayouts::first_line_shift(const core::PreviewBlock &block, float width,
                                     const PreviewTextStyle &style,
                                     std::span<const PreviewInlineImage> inlines)
{
    if (inlines.empty())
    {
        return 0.0F;
    }
    const auto boxed = get(block, width, style, inlines);
    const auto fixed = get(preview_marker_block(block), preview_marker_layout_width(), style);
    const auto boxed_line = boxed ? first_baseline(*boxed.value().Get()) : std::nullopt;
    const auto fixed_line = fixed ? first_baseline(*fixed.value().Get()) : std::nullopt;
    if (!boxed_line.has_value() || !fixed_line.has_value())
    {
        return std::unexpected(directwrite);
    }
    return boxed_line.value() - fixed_line.value();
}

std::expected<float, RenderFailure> PreviewTextLayouts::advance(const core::PreviewBlock &block,
                                                                float width,
                                                                const PreviewTextStyle &style)
{
    const auto layout = get(block, width, style);
    DWRITE_TEXT_METRICS metrics{};
    if (!layout || FAILED(layout.value()->GetMetrics(&metrics)))
    {
        return std::unexpected(directwrite);
    }
    return metrics.widthIncludingTrailingWhitespace;
}

std::expected<float, RenderFailure>
PreviewTextLayouts::natural_width(const core::PreviewBlock &block)
{
    const auto wide = core::to_utf16(block.text);
    if (!wide)
    {
        return std::unexpected(directwrite);
    }
    TextLayout made;
    DWRITE_TEXT_METRICS metrics{};
    const auto ranges = span_ranges(block, block.text);
    if (FAILED(factory_->CreateTextLayout(wide.value().c_str(),
                                          static_cast<UINT32>(wide.value().size()), body_.Get(),
                                          unbounded_height, unbounded_height, &made)) ||
        FAILED(decorate(*made.Get(), block, ranges)) || FAILED(made->GetMetrics(&metrics)))
    {
        return std::unexpected(directwrite);
    }
    ++created_;
    return metrics.width;
}

std::expected<std::vector<float>, RenderFailure>
PreviewTextLayouts::measured_columns(std::span<const core::PreviewBlock> rows)
{
    std::size_t count = 0;
    for (const auto &row : rows)
    {
        count = std::max(count, row.table.has_value() ? row.table.value().cells.size() : 0U);
    }
    std::vector<float> columns;
    columns.reserve(count);
    for (std::size_t column = 0; column < count; ++column)
    {
        const auto width = natural_width(column_block(rows, column));
        if (!width)
        {
            return std::unexpected(width.error());
        }
        columns.push_back(width.value());
    }
    return columns;
}

std::expected<const std::vector<float> *, RenderFailure>
PreviewTextLayouts::table_columns(const core::MarkdownDocument &document, std::size_t index,
                                  const PreviewTextStyle &style)
{
    if (FAILED(prepare(style)))
    {
        return std::unexpected(directwrite);
    }
    // 表の同一性は行が持つ表の要約で 1 回で決める（行の数に依らない・#1852 のレビュー）。
    const auto digest = document.blocks.at(index).table.transform(
        [](const core::PreviewTableRow &row) { return row.digest; });
    const auto found = std::ranges::find_if(
        tables_, [&digest](const TableColumns &entry)
        { return digest.has_value() && std::get<0>(entry) == digest.value(); });
    if (found != tables_.end())
    {
        std::get<1>(*found) = ++uses_;
        return &std::get<2>(*found);
    }
    const auto [first, last] = core::preview_table_rows(document, index);
    auto columns = measured_columns(
        std::span<const core::PreviewBlock>{document.blocks}.subspan(first, last - first));
    if (!columns)
    {
        return std::unexpected(columns.error());
    }
    if (tables_.size() >= table_capacity)
    {
        tables_.erase(std::ranges::min_element(tables_, {}, [](const TableColumns &entry)
                                               { return std::get<1>(entry); }));
    }
    ++measured_tables_;
    tables_.emplace_back(digest.value_or(0U), ++uses_, std::move(columns.value()));
    return &std::get<2>(tables_.back());
}

std::size_t PreviewTextLayouts::measured_tables() const noexcept
{
    return measured_tables_;
}

std::size_t PreviewTextLayouts::held() const noexcept
{
    return held_.size() + cells_.size();
}

std::size_t PreviewTextLayouts::created() const noexcept
{
    return created_;
}

std::size_t PreviewTextLayouts::inked() const noexcept
{
    return inked_;
}

std::size_t PreviewTextLayouts::hit_tests() const noexcept
{
    return hit_tests_;
}
} // namespace nenenib::ui::win32
