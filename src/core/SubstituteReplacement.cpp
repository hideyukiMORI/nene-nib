#include "SubstituteReplacement.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimCase.hpp"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 置き換えの 1 字句の印（code point の外の値）。group_token + n は群 n（0 は当たり）、
// shift_token + n は大小の写像（0 `\u`・1 `\l`・2 `\U`・3 `\L`・4 `\e` `\E`）。ほかの値はその
// 文字を出す。
constexpr char32_t group_token = 0x110000;
constexpr char32_t shift_token = 0x110010;
constexpr char32_t shift_end = shift_token + 5;

// `\` の後ろの文字の字句（`\r` は改行・`\n` は NUL・`\t` は Tab・`\b` は BS・`\0`〜`\9` は群・
// `\u` `\l` `\U` `\L` `\e` `\E` は写像・`\<CR>` は CR の文字・ほかはその文字）。
[[nodiscard]] char32_t escaped_token(char32_t code) noexcept
{
    if (code >= U'0' && code <= U'9')
    {
        return group_token + (code - U'0');
    }
    constexpr std::u32string_view shifts = U"ulULeE";
    const auto shift = shifts.find(code);
    if (shift != std::u32string_view::npos)
    {
        return shift_token + static_cast<char32_t>(std::min<std::size_t>(shift, 4));
    }
    constexpr std::u32string_view abbreviated = U"rntb";
    constexpr std::u32string_view written{U"\n\0\t\b", 4};
    const auto index = abbreviated.find(code);
    return index == std::u32string_view::npos ? code : written.at(index);
}

// at から始まる 1 字句と次の位置（Vim の vim_regsub_both と do_sub の後の歩き）。`&` は当たり、
// CR の文字は改行、末尾の `\` は `\` の文字。
[[nodiscard]] std::pair<char32_t, std::size_t> token_at(std::string_view text, std::size_t at)
{
    const char32_t code = code_point_at(text, Offset{at});
    const std::size_t next = next_code_point(text, Offset{at}).value;
    if (code == U'&')
    {
        return {group_token, next};
    }
    if (code != U'\\' || next >= text.size())
    {
        return {code == U'\r' ? U'\n' : code, next};
    }
    return {escaped_token(code_point_at(text, Offset{next})),
            next_code_point(text, Offset{next}).value};
}

// 大小の写像（Vim の func_one と func_all）。'\0' は写さない、'U' は大文字へ、'L' は小文字へ。
[[nodiscard]] char32_t shifted(char32_t code, char shift) noexcept
{
    if (shift == 'U')
    {
        return vim_to_upper(code);
    }
    return shift == 'L' ? vim_to_lower(code) : code;
}

// 写像の字句を one（次の 1 文字）と all（`\e` まで）に置く。
void shift_switched(char32_t token, char &one, char &all) noexcept
{
    const char32_t kind = token - shift_token;
    constexpr char32_t one_upper = 0;
    constexpr char32_t one_lower = 1;
    constexpr char32_t all_upper = 2;
    constexpr char32_t all_lower = 3;
    if (kind == one_upper || kind == one_lower)
    {
        one = kind == one_upper ? 'U' : 'L';
        return;
    }
    all = kind == all_upper ? 'U' : (kind == all_lower ? 'L' : '\0');
    one = kind == all_upper || kind == all_lower ? one : '\0';
}

// UTF-8 のバイト数（appended を作らずに数える側）。
[[nodiscard]] std::size_t utf8_bytes(char32_t code) noexcept
{
    constexpr char32_t one = 0x80;
    constexpr char32_t two = 0x800;
    constexpr char32_t three = 0x10000;
    if (code < one)
    {
        return 1;
    }
    if (code < two)
    {
        return 2;
    }
    return code < three ? 3 : 4;
}

// 群の本文を 1 文字ずつ put へ（通らなかった群は何も出さない）。
template <typename Put>
void group_put(std::string_view line, const std::optional<VimPatternMatch> &range, Put &put)
{
    if (!range.has_value())
    {
        return;
    }
    for (std::size_t copied = range.value().begin; copied < range.value().end;
         copied = next_code_point(line, Offset{copied}).value)
    {
        put(code_point_at(line, Offset{copied}));
    }
}

// 字句のどれかが accepts に当たるか。
template <typename Accepts> [[nodiscard]] bool any_token(std::string_view text, Accepts accepts)
{
    for (std::size_t at = 0; at < text.size();)
    {
        const auto [token, next] = token_at(text, at);
        if (accepts(token))
        {
            return true;
        }
        at = next;
    }
    return false;
}
} // namespace

SubstituteReplacement::SubstituteReplacement(std::string text, bool literal)
    : text_(std::move(text)), literal_(literal)
{
}

SubstituteReplacement SubstituteReplacement::literal(std::string_view text)
{
    return SubstituteReplacement(std::string(text), true);
}

SubstituteReplacement SubstituteReplacement::vim(std::string_view text)
{
    return SubstituteReplacement(std::string(text), false);
}

template <typename Sink>
void SubstituteReplacement::expanded(std::string_view line, const VimPatternCaptures &captures,
                                     Sink &sink) const
{
    // `\u` `\l` の写像（次の 1 文字で消える）と `\U` `\L` の写像（`\e` `\E` まで）。
    char one = '\0';
    char all = '\0';
    const auto put = [&sink, &one, &all](char32_t code)
    {
        sink(shifted(code, one != '\0' ? one : all));
        one = '\0';
    };
    const std::string_view text = text_;
    for (std::size_t at = 0; at < text.size();)
    {
        const auto [token, next] = token_at(text, at);
        at = next;
        if (token >= shift_token && token < shift_end)
        {
            shift_switched(token, one, all);
            continue;
        }
        if (token < group_token)
        {
            put(token);
            continue;
        }
        group_put(line, captures.at(token - group_token), put);
    }
}

void SubstituteReplacement::appended(std::string &out, std::string_view line,
                                     const VimPatternCaptures &captures) const
{
    if (literal_)
    {
        out += text_;
        return;
    }
    auto sink = [&out](char32_t code) { append_utf8(out, code); };
    expanded(line, captures, sink);
}

std::size_t SubstituteReplacement::expanded_bytes(std::string_view line,
                                                  const VimPatternCaptures &captures,
                                                  std::size_t newline_bytes) const
{
    if (literal_)
    {
        return text_.size();
    }
    std::size_t bytes = 0;
    auto sink = [&bytes, newline_bytes](char32_t code)
    { bytes += code == U'\n' ? newline_bytes : utf8_bytes(code); };
    expanded(line, captures, sink);
    return bytes;
}

bool SubstituteReplacement::reads_subgroups() const noexcept
{
    return !literal_ && any_token(text_, [](char32_t token)
                                  { return token > group_token && token < shift_token; });
}

std::string vim_tilde_expanded(std::string_view replacement, std::string_view previous)
{
    std::string expanded;
    expanded.reserve(replacement.size());
    std::size_t at = 0;
    while (at < replacement.size())
    {
        if (replacement[at] == '~')
        {
            expanded += previous;
            ++at;
            continue;
        }
        const std::size_t length = replacement[at] == '\\' && at + 1 < replacement.size() ? 2 : 1;
        expanded += replacement.substr(at, length);
        at += length;
    }
    return expanded;
}
} // namespace nenenib::core
