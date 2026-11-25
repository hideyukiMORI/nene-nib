// 不可視文字の区間と印の色（ADR 0193 の決定 1・4・5・10・#1977）。
#include "Invisibles.hpp"

#include "Column.hpp"
#include "EditorSettings.hpp"
#include "InvisibleKind.hpp"
#include "InvisibleSpan.hpp"
#include "LineBreakMark.hpp"
#include "LineNumber.hpp"
#include "ListSpace.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"
#include "TextBuffer.hpp"
#include "ThemeDerivation.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
constexpr std::array<ListSpace, 3> list_spaces{ListSpace::none, ListSpace::trail, ListSpace::all};

// 全角空白（U+3000）の UTF-8。
constexpr std::string_view ideographic_space = "\xE3\x80\x80";

[[nodiscard]] bool continuation(char byte) noexcept
{
    return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

// 最後の「空白でも Tab でもない字」の後ろのバイト位置（行末の空白の列の始まり）。
[[nodiscard]] std::size_t trailing_start(std::string_view line) noexcept
{
    std::size_t at = line.size();
    while (at > 0 && (line[at - 1] == ' ' || line[at - 1] == '\t'))
    {
        --at;
    }
    return at;
}

// 空白の種類（trailing は行末の空白の列の中か）。印を付けない空白は値なし。
[[nodiscard]] std::optional<InvisibleKind> space_kind(bool trailing, ListSpace space) noexcept
{
    switch (space)
    {
    case ListSpace::none:
        return std::nullopt;
    case ListSpace::trail:
        return trailing ? std::optional{InvisibleKind::trailing_space} : std::nullopt;
    case ListSpace::all:
        return trailing ? InvisibleKind::trailing_space : InvisibleKind::space;
    }
    std::unreachable();
}

// バイト位置 at の字の種類（印を付けない字は値なし）。trailing は行末の空白の列の中か。
[[nodiscard]] std::optional<InvisibleKind> kind_at(std::string_view line, std::size_t at,
                                                   bool trailing, const EditorSettings &settings)
{
    switch (line[at])
    {
    case ' ':
        return space_kind(trailing, settings.list_space);
    case '	':
        return settings.list_tab ? std::optional{InvisibleKind::tab} : std::nullopt;
    default:
        return settings.list_wide_space && line.substr(at).starts_with(ideographic_space)
                   ? std::optional{InvisibleKind::ideographic_space}
                   : std::nullopt;
    }
}
} // namespace

std::string_view list_space_name(ListSpace space) noexcept
{
    switch (space)
    {
    case ListSpace::none:
        return "none";
    case ListSpace::trail:
        return "trail";
    case ListSpace::all:
        return "all";
    }
    std::unreachable();
}

std::optional<ListSpace> list_space_named(std::string_view name) noexcept
{
    for (const ListSpace space : list_spaces)
    {
        if (list_space_name(space) == name)
        {
            return space;
        }
    }
    return std::nullopt;
}

std::vector<InvisibleSpan> invisible_spans(std::string_view line, const EditorSettings &settings)
{
    std::vector<InvisibleSpan> spans;
    const std::size_t trailing = trailing_start(line);
    std::size_t column = 0;
    for (std::size_t at = 0; at < line.size(); ++at)
    {
        if (continuation(line[at]))
        {
            continue;
        }
        ++column;
        const auto kind = kind_at(line, at, at >= trailing, settings);
        if (!kind.has_value())
        {
            continue;
        }
        if (!spans.empty() && spans.back().kind == kind.value() && spans.back().end.value == column)
        {
            spans.back().end = Column{column + 1};
            continue;
        }
        spans.push_back(InvisibleSpan{Column{column}, Column{column + 1}, kind.value()});
    }
    return spans;
}

std::optional<LineBreakMark> line_break_mark(const TextBuffer &text, LineNumber line)
{
    const std::string ending = text.text_range(text.line_end(line), text.line_terminator_end(line));
    if (ending == "\r\n")
    {
        return LineBreakMark::crlf;
    }
    if (ending == "\n")
    {
        return LineBreakMark::lf;
    }
    return std::nullopt;
}

RgbColor invisible_mark_color(const Palette &palette, InvisibleKind kind) noexcept
{
    switch (kind)
    {
    case InvisibleKind::trailing_space:
    case InvisibleKind::ideographic_space:
        return mix(palette.accent, palette.background, invisible_strong_faded_percent);
    case InvisibleKind::space:
    case InvisibleKind::tab:
        return mix(palette.muted, palette.background, invisible_faded_percent);
    }
    std::unreachable();
}

RgbColor line_break_mark_color(const Palette &palette) noexcept
{
    return mix(palette.muted, palette.background, invisible_faded_percent);
}
} // namespace nenenib::core
