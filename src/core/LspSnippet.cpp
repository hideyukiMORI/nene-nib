#include "LspSnippet.hpp"

#include <cstddef>

namespace nenenib::core
{
namespace
{
[[nodiscard]] bool name_character(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
}

[[nodiscard]] bool escaped(char value) noexcept
{
    return value == '$' || value == '}' || value == '\\';
}

[[nodiscard]] bool digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

// from から続く印の名前の終わり（#1825 のレビュー
// R3）。数字で始まればタブストップで数字だけ、英字か
// `_` で始まれば変数で英数字と `_`。どちらでもなければ from。
[[nodiscard]] std::size_t name_end(std::string_view text, std::size_t from) noexcept
{
    if (from >= text.size() || (!digit(text[from]) && !name_character(text[from])))
    {
        return from;
    }
    const bool tab_stop = digit(text[from]);
    while (from < text.size() && (tab_stop ? digit(text[from]) : name_character(text[from])))
    {
        ++from;
    }
    return from;
}

// at から最初に現れる needle の位置（無ければ text の大きさ）。
[[nodiscard]] std::size_t found_at(std::string_view text, std::size_t at,
                                   std::string_view needle) noexcept
{
    for (; at + needle.size() <= text.size(); ++at)
    {
        if (text.substr(at, needle.size()) == needle)
        {
            return at;
        }
    }
    return text.size();
}

// 選択 `|a,b|}` の `|` の次の at から、最初の選択肢を out に足して `|}` の次を返す。閉じていな
// ければ値なし（印は残す）。
[[nodiscard]] std::size_t first_choice(std::string_view text, std::size_t at, std::string &out)
{
    const std::size_t close = found_at(text, at, "|}");
    if (close == text.size())
    {
        return 0;
    }
    std::size_t end = at;
    while (end < close && text[end] != ',')
    {
        ++end;
    }
    out.append(text.substr(at, end - at));
    return close + 2U;
}

// `${` の後ろ at（名前の始まり）からの印 1 つ。中身を残す印なら open を 1 つ増やす。読んだ後の
// 位置を返し、印でなければ 0。
[[nodiscard]] std::size_t braced(std::string_view text, std::size_t at, std::string &out,
                                 std::size_t &open)
{
    const std::size_t end = name_end(text, at);
    if (end == at || end == text.size())
    {
        return 0;
    }
    switch (text[end])
    {
    case ':':
        ++open;
        return end + 1U;
    case '}':
        return end + 1U;
    case '|':
        return first_choice(text, end + 1U, out);
    default:
        return 0;
    }
}
// `$` の印（at は `$` の位置で、後ろに 1 文字ある）。読んだ後の位置を返し、印でなければ at + 1
// 以下。
[[nodiscard]] std::size_t dollar_mark(std::string_view text, std::size_t at, std::string &out,
                                      std::size_t &open)
{
    return text[at + 1U] == '{' ? braced(text, at + 2U, out, open) : name_end(text, at + 1U);
}

// index の 1 文字か印 1 つを読んで out に足し、次の位置を返す。
[[nodiscard]] std::size_t snippet_step(std::string_view snippet, std::size_t index,
                                       std::string &out, std::size_t &open)
{
    const char character = snippet[index];
    const bool has_next = index + 1U < snippet.size();
    if (character == '\\' && has_next && escaped(snippet[index + 1U]))
    {
        out.push_back(snippet[index + 1U]);
        return index + 2U;
    }
    const std::size_t mark =
        character == '$' && has_next ? dollar_mark(snippet, index, out, open) : 0;
    if (mark > index + 1U)
    {
        return mark;
    }
    if (character == '}' && open > 0)
    {
        --open;
        return index + 1U;
    }
    out.push_back(character);
    return index + 1U;
}
} // namespace

std::string lsp_snippet_text(std::string_view snippet)
{
    std::string out;
    out.reserve(snippet.size());
    std::size_t open = 0;
    std::size_t index = 0;
    while (index < snippet.size())
    {
        index = snippet_step(snippet, index, out, open);
    }
    return out;
}
} // namespace nenenib::core
