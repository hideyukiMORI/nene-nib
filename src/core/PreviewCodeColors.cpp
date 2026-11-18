#include "PreviewCodeColors.hpp"

#include "AsciiCase.hpp"
#include "LanguageTable.hpp"
#include "Lexer.hpp"
#include "Utf8.hpp"

#include <string>

namespace nenenib::core
{
namespace
{
// 1 行（line・行頭が text の begin）の色の区間を、桁（code point の序数・1 始まり）から text の
// バイト位置に写して足す。区間は桁の昇順なので、行を前から 1 回だけ歩く。
void append_line(std::vector<PreviewCodeColor> &colors, std::string_view line, std::size_t begin,
                 const std::vector<ColorSpan> &spans)
{
    std::size_t column = 1;
    Offset at{0};
    const auto advance_to = [&](std::size_t target)
    {
        while (column < target && at.value < line.size())
        {
            at = next_code_point(line, at);
            ++column;
        }
        return at.value;
    };
    for (const auto &span : spans)
    {
        const auto from = advance_to(span.begin.value);
        const auto to = advance_to(span.end.value);
        if (to > from)
        {
            colors.push_back(PreviewCodeColor{Offset{begin + from}, Offset{begin + to}, span.role});
        }
    }
}

// 囲みのコードの要素の text は行ごとに '\n' で終わるので、改行の数が行の数。
[[nodiscard]] std::size_t newline_count(std::string_view text) noexcept
{
    std::size_t lines = 0;
    for (const char byte : text)
    {
        lines += byte == '\n' ? 1U : 0U;
    }
    return lines;
}
} // namespace

std::optional<Language> preview_code_language(std::string_view word)
{
    std::string folded;
    folded.reserve(word.size());
    for (const char byte : word)
    {
        folded.push_back(static_cast<char>(ascii_folded(byte)));
    }
    const auto language = language_of_label(folded);
    if (!language.has_value() || language.value() == Language::markdown ||
        !has_lexer(language.value()))
    {
        return std::nullopt;
    }
    return language;
}

std::optional<std::vector<PreviewCodeColor>> preview_code_colors(Language language,
                                                                 std::string_view text)
{
    if (text.size() > preview_code_color_byte_limit ||
        newline_count(text) > preview_code_color_line_limit)
    {
        return std::nullopt;
    }
    std::vector<PreviewCodeColor> colors;
    auto state = lex_start_state(language, true);
    std::size_t begin = 0;
    while (begin < text.size())
    {
        std::size_t end = begin;
        while (end < text.size() && text[end] != '\n')
        {
            ++end;
        }
        const auto line = text.substr(begin, end - begin);
        auto lexed = lex_line(language, state, line);
        append_line(colors, line, begin, lexed.spans);
        state = lexed.out;
        begin = end + 1U;
    }
    return colors;
}
} // namespace nenenib::core
