#include "VimCaret.hpp"

#include "LineNumber.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
// キャレットの寄せが読む窓の片側の初めの大きさ（バイト）。行の全体は写さない（ADR 0053 の決定 5）。
constexpr std::size_t caret_window_bytes = 64;
// UTF-8 の 1 code point の最長のバイト数。窓の終わりで切れた code point を読まないための余白。
constexpr std::size_t longest_code_point_bytes = 4;

[[nodiscard]] bool blank_byte(char value) noexcept
{
    return value == ' ' || value == '\t';
}

// content の column のバイトを含む文字の終わり（次の文字の先頭）。column が content の終わりなら
// そのまま。
[[nodiscard]] Offset character_end_containing(std::string_view content, Offset column) noexcept
{
    if (column.value >= content.size())
    {
        return column;
    }
    return vim_character_end(
        content, is_boundary(content, column) ? column : previous_code_point(content, column));
}

// 行 [line_start, line_end) の at のバイトを含む Vim の文字の先頭。at が line_end なら行の最後の
// 文字の先頭（ADR 0053 の決定 5）。読むのは at の周りの窓だけ（行の全体は写さない）。見つけた文字の
// 先頭が窓の先頭なら文字はもっと手前から始まるかもしれないので手前へ、文字の終わりが窓の終わりに
// 近ければ後ろへ、窓を倍に広げて読み直す（長い結合文字の列でも正しい）。行は空でない。
[[nodiscard]] Offset character_near(const TextBuffer &text, Offset line_start, Offset line_end,
                                    Offset at)
{
    std::size_t before = caret_window_bytes;
    std::size_t after = caret_window_bytes;
    for (;;)
    {
        const Offset begin{at.value - std::min(before, at.value - line_start.value)};
        const Offset end{at.value + std::min(after, line_end.value - at.value)};
        const std::string content = text.text_range(begin, end);
        const Offset character_end =
            character_end_containing(content, Offset{at.value - begin.value});
        const Offset start = vim_character_start(content, character_end);
        if (!(end == line_end) && character_end.value + longest_code_point_bytes > content.size())
        {
            after *= 2;
        }
        else if (!(begin == line_start) && start.value == 0)
        {
            before *= 2;
        }
        else
        {
            return Offset{begin.value + start.value};
        }
    }
}
} // namespace

Offset vim_resting_caret(const TextBuffer &text, Offset caret)
{
    const LineNumber line = text.position_of(caret).line;
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    if (end.value <= start.value)
    {
        return start;
    }
    // 文字の途中（結合文字の上）には立たない。行末を越えたら行の最後の文字の先頭（ADR 0053 の
    // 決定 5）。
    return character_near(text, start, end, caret.value < end.value ? caret : end);
}

Offset vim_first_non_blank(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.position_of(at).line;
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    std::size_t index = 0;
    while (index < content.size() && blank_byte(content[index]))
    {
        ++index;
    }
    if (index >= content.size())
    {
        // 空白しか無い行では Vim の ^ と同じく最後の文字の上に載る（空行なら行頭）。
        return vim_resting_caret(text, text.line_end(line));
    }
    return Offset{start.value + index};
}

Offset vim_line_and_column(const TextBuffer &text, LineNumber line, std::size_t column)
{
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    if (column >= end.value - start.value)
    {
        return end;
    }
    return character_near(text, start, end, Offset{start.value + column});
}

Offset vim_same_line_and_column(const TextBuffer &before, Offset at, const TextBuffer &after)
{
    const LineNumber line = before.position_of(at).line;
    const std::size_t column = at.value - before.line_start(line).value;
    if (line.value > after.line_count())
    {
        return vim_first_non_blank(after, after.line_start(LineNumber{after.line_count()}));
    }
    return vim_line_and_column(after, line, column);
}
} // namespace nenenib::core
