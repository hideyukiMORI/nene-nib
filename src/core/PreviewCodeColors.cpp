#include "PreviewCodeColors.hpp"

#include "AsciiCase.hpp"
#include "LanguageTable.hpp"
#include "Lexer.hpp"
#include "Utf8.hpp"

#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// 区間を足す。直前の区間と同じ役で間が空いていなければ 1 つに畳む（間に文字をはさむ区間は
// 畳まない・#1860 のレビュー M1）。
void place(std::vector<PreviewCodeColor> &colors, PreviewCodeColor color)
{
    if (!colors.empty() && colors.back().role == color.role && colors.back().end == color.begin)
    {
        colors.back().end = color.end;
        return;
    }
    colors.push_back(color);
}

// 1 行（line・行頭が切れの text の begin）の色の区間を、桁（code point の序数・1 始まり）から切れの
// text のバイト位置に写して足す。区間は桁の昇順なので、行を前から 1 回だけ歩く。畳んだ後の区間が
// 上限を超えた時点で止めて false（その切れは 1 色・1 行だけの巨大な囲みでも区間を溜めない・
// review-1860b の L1）。
[[nodiscard]] bool append_line(std::vector<PreviewCodeColor> &colors, std::string_view line,
                               std::size_t begin, const std::vector<ColorSpan> &spans)
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
            place(colors, PreviewCodeColor{Offset{begin + from}, Offset{begin + to}, span.role});
        }
        if (colors.size() > preview_code_color_count_limit)
        {
            return false;
        }
    }
    return true;
}

// 行の終わり（'\n' の位置か text の終わり）。1 バイトずつ歩く（STL の検索は Debug で memchr を
// core の外へ出す・ARC-003・#784）。
[[nodiscard]] std::size_t line_end_of(std::string_view text, std::size_t begin) noexcept
{
    while (begin < text.size() && text[begin] != '\n')
    {
        ++begin;
    }
    return begin;
}

// 1 つの切れ（piece）の色の区間。state は切れの前の行が渡した状態で、切れの最後の行の後の状態に
// 進める。上限を超えた後の行は状態だけを進める（区間の vector を確保しない lex_state）。
[[nodiscard]] std::optional<std::vector<PreviewCodeColor>>
piece_colors(Language language, LexState &state, std::string_view piece)
{
    std::optional<std::vector<PreviewCodeColor>> colors{std::in_place};
    for (std::size_t begin = 0; begin < piece.size();)
    {
        const auto end = line_end_of(piece, begin);
        const auto line = piece.substr(begin, end - begin);
        begin = end + 1U;
        if (!colors.has_value())
        {
            state = lex_state(language, state, line);
            continue;
        }
        auto lexed = lex_line(language, state, line);
        state = lexed.out;
        if (!append_line(colors.value(), line, end - line.size(), lexed.spans))
        {
            colors.reset();
        }
    }
    return colors;
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
    // `rust,ignore` `cpp,linenos` は `,` の前だけを見る（#1860 のレビュー L2）。
    std::string folded;
    folded.reserve(word.size());
    for (const char byte : word)
    {
        if (byte == ',')
        {
            break;
        }
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

std::vector<std::size_t> preview_code_piece_starts(std::string_view text)
{
    std::vector<std::size_t> starts{0U};
    std::size_t lines = 0;
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        if (text[at] != '\n')
        {
            continue;
        }
        ++lines;
        if (lines % preview_code_piece_lines == 0U && at + 1U < text.size())
        {
            starts.push_back(at + 1U);
        }
    }
    return starts;
}

std::vector<std::optional<std::vector<PreviewCodeColor>>>
preview_code_piece_colors(Language language, std::string_view text,
                          const std::vector<std::size_t> &starts)
{
    std::vector<std::optional<std::vector<PreviewCodeColor>>> pieces(starts.size());
    if (text.size() > preview_code_color_byte_limit ||
        newline_count(text) > preview_code_color_line_limit)
    {
        return pieces;
    }
    // 囲みは文書の断片なので、遡りの途中から読むときの始まりの状態で読む（PHP は `<?php` が
    // 無くても PHP の中から・ほかの言語は LexState{} で同じ・#1860 のレビュー R3）。
    auto state = lex_start_state(language, false);
    for (std::size_t piece = 0; piece < starts.size(); ++piece)
    {
        const auto end = piece + 1U < starts.size() ? starts.at(piece + 1U) : text.size();
        pieces.at(piece) =
            piece_colors(language, state, text.substr(starts.at(piece), end - starts.at(piece)));
    }
    return pieces;
}
} // namespace nenenib::core
