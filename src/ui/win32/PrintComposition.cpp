#include "PrintComposition.hpp"

#include "DevicePixels.hpp"
#include "DisplayLine.hpp"
#include "GutterMetrics.hpp"
#include "LexLine.hpp"
#include "Lexer.hpp"
#include "PrintPages.hpp"
#include "PrintSheet.hpp"
#include "SyntaxColor.hpp"
#include "TextLayoutRequest.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
constexpr std::size_t surrogate_pair_bytes = 4;
constexpr auto dpi = core::reference_dpi;

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

// 描画の文字列の code point ごとの UTF-16 の始まり（大きさは code point の数 + 1・末尾は全長）。
[[nodiscard]] std::vector<UINT32> utf16_starts(std::string_view display)
{
    std::vector<UINT32> starts;
    starts.reserve(display.size() + 1);
    UINT32 units = 0;
    for (std::size_t offset = 0; offset < display.size();)
    {
        starts.push_back(units);
        const auto next = core::next_code_point(display, core::Offset{offset}).value;
        units += next - offset == surrogate_pair_bytes ? 2U : 1U;
        offset = next;
    }
    starts.push_back(units);
    return starts;
}

// 本文の桁の区間を描画の UTF-16 の範囲へ写す（画面の renderer の range_of と同じ写し・ADR 0040）。
[[nodiscard]] DWRITE_TEXT_RANGE shown_range(const core::DisplayLine &line,
                                            const std::vector<UINT32> &starts, std::size_t from,
                                            std::size_t to)
{
    const auto begin = starts.at(std::min(core::display_position(line, from), starts.size() - 1));
    const auto end = starts.at(std::min(core::display_position(line, to), starts.size() - 1));
    return DWRITE_TEXT_RANGE{begin, end > begin ? end - begin : 0U};
}

[[nodiscard]] std::vector<ColoredRun> colored_runs(const core::DisplayLine &line,
                                                   const std::vector<UINT32> &starts,
                                                   const std::vector<core::ColorSpan> &spans,
                                                   const core::SyntaxPalette &palette)
{
    std::vector<ColoredRun> colored;
    colored.reserve(spans.size());
    for (const auto &span : spans)
    {
        const auto range = shown_range(line, starts, span.begin.value - 1, span.end.value - 1);
        if (range.length > 0)
        {
            colored.push_back(ColoredRun{range, to_color(core::syntax_color(palette, span.role))});
        }
    }
    return colored;
}

[[nodiscard]] std::vector<DWRITE_TEXT_RANGE> replaced_ranges(const core::DisplayLine &line,
                                                             const std::vector<UINT32> &starts)
{
    std::vector<DWRITE_TEXT_RANGE> ranges;
    for (std::size_t column = 0; column + 1 < line.starts.size(); ++column)
    {
        if (core::is_replaced(line, column))
        {
            ranges.push_back(shown_range(line, starts, column, column + 1));
        }
    }
    return ranges;
}

// 本文の次の 1 行（改行を含まない）と次の行の始まり。CRLF の文書の行末の `\r` は改行の一部
// （ADR 0036 の決定 2）。最後の行の次は npos。
[[nodiscard]] std::pair<std::string_view, std::size_t>
next_line(std::string_view text, std::size_t start, core::LineEnding ending)
{
    const auto stop = text.find('\n', start);
    auto line =
        text.substr(start, stop == std::string_view::npos ? std::string_view::npos : stop - start);
    if (ending == core::LineEnding::crlf && stop != std::string_view::npos && line.ends_with('\r'))
    {
        line.remove_suffix(1);
    }
    return {line, stop == std::string_view::npos ? stop : stop + 1};
}

[[nodiscard]] bool exists(IDWriteFactory &factory, const std::wstring &name)
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
    UINT32 index = 0;
    BOOL present = FALSE;
    return SUCCEEDED(factory.GetSystemFontCollection(&collection)) &&
           SUCCEEDED(collection->FindFamilyName(name.c_str(), &index, &present)) &&
           present != FALSE;
}
} // namespace

PrintComposition::PrintComposition(core::EditorSettings settings, PrintPaper paper, bool gutter)
    : settings_(std::move(settings)), paper_(paper), palette_(core::print_palette()),
      layouts_(std::make_unique<BodyTextLayouts>())
{
    const auto margin = static_cast<std::int32_t>(core::print_margin(dpi));
    const auto line = core::body_layout_in({0, 0, 1, 1}, dpi, settings_.font_size).line_height;
    header_ =
        core::LayoutRect{margin, margin, std::max(paper.width - margin, margin), margin + line};
    body_ = core::body_layout_in(core::LayoutRect{margin, header_.bottom, header_.right,
                                                  std::max(paper.height - margin, header_.bottom)},
                                 dpi, settings_.font_size);
    if (!gutter)
    {
        body_.gutter.right = body_.gutter.left;
        body_.content.left = body_.band.left;
    }
}

std::expected<PrintComposition, application::PrintFailure>
PrintComposition::compose(const application::PrintJob &job, PrintPaper paper)
{
    auto settings = core::default_editor_settings();
    settings.font_family = job.family;
    settings.font_size = job.size;
    // 印刷は紙の幅で折り返す（折り返さない印刷は後続・決定 9）。
    settings.wrapping = core::LineWrapping::wrap;
    PrintComposition composition(std::move(settings), paper, job.gutter);
    composition.title_ = std::string(job.title.text());
    Microsoft::WRL::ComPtr<IUnknown> created;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   created.GetAddressOf())) ||
        FAILED(created.As(&composition.factory_)) || !composition.add_lines(job) ||
        (job.gutter && !composition.make_gutter_format()))
    {
        return std::unexpected(application::PrintFailure::rendering_failed);
    }
    const auto &body = composition.body_;
    const auto margin = core::print_margin(dpi);
    // ヘッダの高さは上の余白から本文の最初の行まで（ヘッダの 1 行と本文の帯の上の間）。本文の行の
    // 高さは layout の UNIFORM の行間と同じ整数（BodyTextLayouts::set_spacing）なので割り算が合う。
    composition.rows_per_page_ = core::print_rows_per_page(
        core::PrintSheet{static_cast<std::uint32_t>(std::max(paper.height, 0)), dpi,
                         static_cast<std::uint32_t>(
                             std::max(body.content.top - static_cast<std::int32_t>(margin), 0)),
                         static_cast<std::uint32_t>(body.line_height)});
    const auto &last = composition.lines_.back();
    composition.pages_ = core::print_pages(last.first_row + last.rows,
                                           static_cast<std::uint32_t>(composition.rows_per_page_));
    return composition;
}

bool PrintComposition::add_lines(const application::PrintJob &job)
{
    const std::string_view text = job.text ? std::string_view(*job.text) : std::string_view{};
    const bool colored = core::has_lexer(job.language);
    auto state = core::lex_start_state(job.language, true);
    const core::TextViewGeometry geometry{std::max(core::width_of(body_.content), 1),
                                          body_.line_height, dpi};
    std::size_t row = 0;
    for (std::size_t start = 0, number = 1; start != std::string_view::npos; ++number)
    {
        const auto [raw, next] = next_line(text, start, job.line_ending);
        start = next;
        auto lexed = colored ? core::lex_line(job.language, state, raw) : core::LexLine{{}, state};
        state = lexed.out;
        auto display = core::display_line(raw);
        const auto starts = utf16_starts(display.text);
        const auto rows =
            layouts_->rows(application::TextLayoutRequest{display.text, settings_, geometry});
        if (!rows)
        {
            return false;
        }
        lines_.push_back(PrintLine{core::LineNumber{number}, display.text,
                                   colored_runs(display, starts, lexed.spans, palette_.body),
                                   replaced_ranges(display, starts), row, rows.value()});
        row += rows.value();
    }
    return true;
}

bool PrintComposition::make_gutter_format()
{
    const auto requested = core::to_utf16(settings_.font_family.text());
    const auto face = requested && exists(*factory_.Get(), requested.value())
                          ? requested.value()
                          : std::wstring{L"Consolas"};
    const float size = gutter_text_dips * core::font_size_ratio(settings_.font_size);
    return SUCCEEDED(factory_->CreateTextFormat(face.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                                DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, size, L"", &gutter_)) &&
           SUCCEEDED(gutter_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING)) &&
           SUCCEEDED(gutter_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)) &&
           SUCCEEDED(gutter_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
}

std::size_t PrintComposition::page_count() const noexcept
{
    return pages_.size();
}

PrintPageInput PrintComposition::page(std::size_t index)
{
    return PrintPageInput{
        pages_.at(index), index + 1,       pages_.size(),
        lines_,           title_,          core::LayoutRect{0, 0, paper_.width, paper_.height},
        header_,          body_,           settings_,
        *layouts_,        *factory_.Get(), gutter_.Get(),
        palette_};
}

D2D1_SIZE_F PrintComposition::paper_size() const noexcept
{
    return D2D1::SizeF(static_cast<float>(paper_.width), static_cast<float>(paper_.height));
}

const core::BodyLayout &PrintComposition::body() const noexcept
{
    return body_;
}

std::size_t PrintComposition::rows_per_page() const noexcept
{
    return rows_per_page_;
}
} // namespace nenenib::ui::win32
