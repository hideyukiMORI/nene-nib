#include "PreviewLines.hpp"

#include "FinderPreview.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::application
{
namespace
{
constexpr char space = ' ';
constexpr unsigned char last_control = 0x1F;
constexpr unsigned char delete_character = 0x7F;

// 行の先頭から最大 finder_preview_line_bytes バイト（コードポイントを割らない）。制御文字（Tab を
// 除く）は 1 バイトの空白に（どれも UTF-8 で 1 バイトなので長さと位置は変わらない）。
[[nodiscard]] std::string clipped_line(const core::TextBuffer &text, core::LineNumber line)
{
    const auto begin = text.line_start(line).value;
    const auto end = std::min(text.line_end(line).value, begin + core::finder_preview_line_bytes);
    std::string bytes = text.text_range(core::Offset{begin}, core::Offset{end});
    bytes.resize(core::whole_code_points(bytes));
    for (char &letter : bytes)
    {
        const auto code = static_cast<unsigned char>(letter);
        if ((code <= last_control && letter != '\t') || code == delete_character)
        {
            letter = space;
        }
    }
    return bytes;
}

// 窓の最初の行（1 始まり）。当たりがあればその前 finder_preview_context_lines 行から。
[[nodiscard]] std::size_t first_line(const core::TextBuffer &text,
                                     const std::optional<PreviewAnchor> &anchor) noexcept
{
    if (!anchor.has_value() || anchor.value().line.value == 0 ||
        anchor.value().line.value > text.line_count())
    {
        return 1;
    }
    const auto line = anchor.value().line.value;
    return line > core::finder_preview_context_lines ? line - core::finder_preview_context_lines
                                                     : 1;
}
} // namespace

std::vector<PreviewLine> preview_lines(const core::TextBuffer &text,
                                       const std::optional<PreviewAnchor> &anchor)
{
    std::vector<PreviewLine> lines;
    const auto count = text.line_count();
    const auto first = first_line(text, anchor);
    const auto last = std::min(count, first + core::finder_preview_line_limit - 1);
    lines.reserve(last >= first ? last - first + 1 : 0);
    for (std::size_t number = first; number <= last; ++number)
    {
        PreviewLine line{core::LineNumber{number}, clipped_line(text, core::LineNumber{number})};
        if (anchor.has_value() && anchor.value().line.value == number)
        {
            line.hit = true;
            line.match_begin = std::min(anchor.value().match_begin, line.text.size());
            line.match_end =
                std::clamp(anchor.value().match_end, line.match_begin, line.text.size());
        }
        lines.push_back(std::move(line));
    }
    return lines;
}
} // namespace nenenib::application
