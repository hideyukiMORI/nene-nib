#include "PreviewLines.hpp"

#include "ColorSpan.hpp"
#include "DisplayLine.hpp"
#include "FinderPreview.hpp"
#include "LexState.hpp"
#include "Lexer.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::application
{
namespace
{
constexpr unsigned char continuation_mask = 0xC0;
constexpr unsigned char continuation_bits = 0x80;

// 行の先頭から最大 finder_preview_line_bytes バイト（コードポイントを割らない）。
[[nodiscard]] std::string clipped_line(const core::TextBuffer &text, core::LineNumber line)
{
    const auto begin = text.line_start(line).value;
    const auto end = std::min(text.line_end(line).value, begin + core::finder_preview_line_bytes);
    std::string bytes = text.text_range(core::Offset{begin}, core::Offset{end});
    bytes.resize(core::whole_code_points(bytes));
    return bytes;
}

// 描画の形の各コードポイントが始まるバイトの位置（末尾に全体の長さ）。
[[nodiscard]] std::vector<std::size_t> byte_starts(std::string_view shown)
{
    std::vector<std::size_t> starts;
    for (std::size_t at = 0; at < shown.size(); ++at)
    {
        if ((static_cast<unsigned char>(shown[at]) & continuation_mask) != continuation_bits)
        {
            starts.push_back(at);
        }
    }
    starts.push_back(shown.size());
    return starts;
}

// 描画の形と、その各コードポイントが始まるバイトの位置。
using Shown = std::pair<core::DisplayLine, std::vector<std::size_t>>;

// 本文の桁（0 始まりのコードポイントの序数）→ 描画の形のバイトの位置。
[[nodiscard]] std::size_t shown_byte(const Shown &shown, std::size_t column)
{
    const auto position = core::display_position(shown.first, column);
    return shown.second.at(std::min(position, shown.second.size() - 1));
}

// 本文のバイトの位置 → 0 始まりのコードポイントの序数。
[[nodiscard]] std::size_t column_of(std::string_view raw, std::size_t byte)
{
    return core::code_point_count(raw.substr(0, std::min(byte, raw.size())));
}

// 窓の最初の行（1 始まり）。当たりがあればその前 finder_preview_context_lines 行から。
[[nodiscard]] std::size_t first_line(std::size_t count,
                                     const std::optional<PreviewAnchor> &anchor) noexcept
{
    if (!anchor.has_value() || anchor.value().line.value == 0 || anchor.value().line.value > count)
    {
        return 1;
    }
    const auto line = anchor.value().line.value;
    return line > core::finder_preview_context_lines ? line - core::finder_preview_context_lines
                                                     : 1;
}

// 窓に入れる行の数（改行で終わる本文の最後の空の行は数えない）。
[[nodiscard]] std::size_t shown_count(const core::TextBuffer &text) noexcept
{
    const auto count = text.line_count();
    return count > 1 && text.final_line_break_bytes() > 0 ? count - 1 : count;
}

// 構文の色の区間を描画の形のバイトへ写して line に置き、次の行へ渡す状態を進める。
void color_line(std::string_view raw, const Shown &shown, PreviewLine &line,
                std::pair<core::Language, core::LexState> &lexing)
{
    if (!core::has_lexer(lexing.first))
    {
        return;
    }
    auto lexed = core::lex_line(lexing.first, lexing.second, raw);
    lexing.second = lexed.out;
    for (const core::ColorSpan &span : lexed.spans)
    {
        line.colors.push_back(PreviewColor{shown_byte(shown, span.begin.value - 1),
                                           shown_byte(shown, span.end.value - 1), span.role});
    }
}

// 1 行の描画の形・構文の色・当たりの印。lexing は言語と次の行へ渡す字句の状態。
[[nodiscard]] PreviewLine shown_line(std::string_view raw, core::LineNumber number,
                                     const std::optional<PreviewAnchor> &anchor,
                                     std::pair<core::Language, core::LexState> &lexing)
{
    auto display = core::display_line(raw);
    auto starts = byte_starts(display.text);
    const Shown shown{std::move(display), std::move(starts)};
    PreviewLine line{number, shown.first.text};
    color_line(raw, shown, line, lexing);
    if (anchor.has_value() && anchor.value().line == number)
    {
        line.hit = true;
        line.match_begin = shown_byte(shown, column_of(raw, anchor.value().match_begin));
        line.match_end =
            std::max(line.match_begin, shown_byte(shown, column_of(raw, anchor.value().match_end)));
    }
    return line;
}
} // namespace

std::vector<PreviewLine> preview_lines(const core::TextBuffer &text,
                                       const std::optional<PreviewAnchor> &anchor,
                                       core::Language language)
{
    std::vector<PreviewLine> lines;
    const auto count = shown_count(text);
    const auto first = first_line(count, anchor);
    const auto last = std::min(count, first + core::finder_preview_line_limit - 1);
    lines.reserve(last >= first ? last - first + 1 : 0);
    std::pair<core::Language, core::LexState> lexing{language,
                                                     core::lex_start_state(language, first == 1)};
    for (std::size_t number = first; number <= last; ++number)
    {
        const core::LineNumber line{number};
        lines.push_back(shown_line(clipped_line(text, line), line, anchor, lexing));
    }
    return lines;
}
} // namespace nenenib::application
