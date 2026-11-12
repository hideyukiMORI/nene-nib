#include "ExSubstituteCommand.hpp"

#include <charconv>
#include <cstddef>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view substitute_name = "substitute";
// 回数の上限（Vim の INT_MAX・これ以上は E1510）。
constexpr std::size_t substitute_count_limit = 2147483647;
// `:s` の直後がこの文字なら前のパターンと置き換え（Vim の ex_substitute の判定）。
constexpr std::string_view previous_starts = "0123456789cegriIp|\"";
// フラグの文字（Vim の ex_substitute の while）。
constexpr std::string_view flag_letters = "gcneriIp#l";
// 段 1 が受けないフラグ（`c` は段 2・`p` `#` `l` は表示・ADR 0168 の決定 6・10）。
constexpr std::string_view unsupported_flags = "cp#l";

[[nodiscard]] bool is_letter(char letter) noexcept
{
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z');
}

[[nodiscard]] char at_or_nul(std::string_view text, std::size_t at) noexcept
{
    return at < text.size() ? text[at] : '\0';
}

// letter が set の 1 文字か（STL の検索は core の外へ memchr を出すので歩く・ARC-003）。
[[nodiscard]] bool one_of(std::string_view set, char letter) noexcept
{
    for (const char entry : set)
    {
        if (entry == letter)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string_view skipped_white(std::string_view text) noexcept
{
    std::size_t first = 0;
    while (first < text.size() && (text[first] == ' ' || text[first] == '\t'))
    {
        ++first;
    }
    return text.substr(first);
}

// Vim の find_ex_command の特例: `:sg` `:si` `:sI` `:sc` `:sr` は `:s` に続くフラグ。
[[nodiscard]] bool flagged_s(std::string_view text) noexcept
{
    if (text.size() < 2 || text[0] != 's')
    {
        return false;
    }
    const char second = text[1];
    const char third = at_or_nul(text, 2);
    return (second == 'c' && third != 's' && third != 'r' &&
            (at_or_nul(text, 3) != 'i' || at_or_nul(text, 4) != 'p')) ||
           second == 'g' || (second == 'i' && third != 'm' && third != 'l' && third != 'g') ||
           second == 'I' || (second == 'r' && third != 'e');
}

// `[` の後ろの集合を飛ばした位置（Vim の skip_anyof の部分集合）。閉じが無ければ末尾。
[[nodiscard]] std::size_t skipped_collection(std::string_view text, std::size_t at) noexcept
{
    std::size_t next = at + 1;
    if (at_or_nul(text, next) == '^')
    {
        ++next;
    }
    if (at_or_nul(text, next) == ']')
    {
        ++next;
    }
    while (next < text.size() && text[next] != ']')
    {
        next += text[next] == '\\' && next + 1 < text.size() ? 2 : 1;
    }
    return next < text.size() ? next + 1 : text.size();
}

// 区切りまでのパターンの終わり（Vim の skip_regexp_ex）。`\` の次と `[...]` の中の区切りは終わりに
// しない。区切りが無ければ末尾。
[[nodiscard]] std::size_t pattern_end(std::string_view text, char delimiter) noexcept
{
    std::size_t at = 0;
    while (at < text.size() && text[at] != delimiter)
    {
        if (text[at] == '[')
        {
            at = skipped_collection(text, at);
            continue;
        }
        at += text[at] == '\\' && at + 1 < text.size() ? 2 : 1;
    }
    return at;
}

// 区切りまでの置き換えの終わり（Vim の skip_substitute）。`\` の次は飛ばす。
[[nodiscard]] std::size_t replacement_end(std::string_view text, char delimiter) noexcept
{
    std::size_t at = 0;
    while (at < text.size() && text[at] != delimiter)
    {
        at += text[at] == '\\' && at + 1 < text.size() ? 2 : 1;
    }
    return at;
}

// 区切りの後ろの区切りを 1 つ食べた残り。
[[nodiscard]] std::string_view after_delimiter(std::string_view text, std::size_t end) noexcept
{
    return end < text.size() ? text.substr(end + 1) : std::string_view{};
}

// フラグの文字の長さ。段 1 が受けない文字があれば unsupported_argument。
[[nodiscard]] std::expected<std::size_t, ExFailure> flag_length(std::string_view text) noexcept
{
    std::size_t letters = 0;
    while (letters < text.size() && one_of(flag_letters, text[letters]))
    {
        if (one_of(unsupported_flags, text[letters]))
        {
            return std::unexpected(ExFailure::unsupported_argument);
        }
        ++letters;
    }
    return letters;
}

// 回数（先頭の数字・無ければ値なし・大きすぎれば size_t の最大）と、その長さ。
[[nodiscard]] std::pair<std::optional<std::size_t>, std::size_t>
count_read(std::string_view text) noexcept
{
    std::size_t digits = 0;
    while (digits < text.size() && text[digits] >= '0' && text[digits] <= '9')
    {
        ++digits;
    }
    if (digits == 0)
    {
        return {std::nullopt, 0};
    }
    std::size_t count = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + digits, count);
    return {parsed.ec == std::errc{} ? count : std::numeric_limits<std::size_t>::max(), digits};
}

// フラグ・回数・後ろの余り（Vim の ex_substitute の後半）を request に読む。
[[nodiscard]] std::expected<ExSubstituteRequest, ExFailure> flags_read(std::string_view text,
                                                                       ExSubstituteRequest request)
{
    request.keep_flags = !text.empty() && text.front() == '&';
    text.remove_prefix(request.keep_flags ? 1 : 0);
    const auto letters = flag_length(text);
    if (!letters)
    {
        return std::unexpected(letters.error());
    }
    request.flags = std::string(text.substr(0, letters.value()));
    text = skipped_white(text.substr(letters.value()));
    const auto [count, digits] = count_read(text);
    // 回数 0 は `e` の有無で E939 か何もしないかが分かれ、`e` は `&` で前のフラグからも来るので
    // engine が決める。INT_MAX 以上は E1510（Vim の ex_substitute・2147483646
    // までは受ける・実測）。
    if (count.value_or(0) >= substitute_count_limit)
    {
        return std::unexpected(ExFailure::value_too_large);
    }
    request.count = count;
    text = skipped_white(text.substr(digits));
    // `|` の後ろの命令を続けて流すのは Nib ではまだ受けない（Vim は次の命令にする）。
    if (text.starts_with('|'))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    if (!text.empty() && text.front() != '"')
    {
        return std::unexpected(ExFailure::trailing_characters);
    }
    return request;
}

// `:s` の引数（名前の後ろ・先頭の空白は落とした）。
[[nodiscard]] std::expected<ExSubstituteRequest, ExFailure>
substitute_argument(std::string_view text)
{
    ExSubstituteRequest request{std::nullopt, ExSubstituteSource::repeated, '/', {}, {}, false, {},
                                std::nullopt};
    if (text.empty() || one_of(previous_starts, text.front()))
    {
        return flags_read(text, std::move(request));
    }
    const char delimiter = text.front();
    if (is_letter(delimiter))
    {
        return std::unexpected(ExFailure::letter_delimiter);
    }
    const auto byte = static_cast<unsigned char>(delimiter);
    constexpr unsigned char ascii_limit = 0x80;
    if (delimiter == '\\' || byte >= ascii_limit)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    request.source = ExSubstituteSource::typed;
    request.delimiter = delimiter;
    text.remove_prefix(1);
    const std::size_t pattern = pattern_end(text, delimiter);
    request.pattern = std::string(text.substr(0, pattern));
    text = after_delimiter(text, pattern);
    const std::size_t replacement = replacement_end(text, delimiter);
    request.replacement = std::string(text.substr(0, replacement));
    if (request.replacement.starts_with("\\="))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return flags_read(after_delimiter(text, replacement), std::move(request));
}
} // namespace

std::optional<std::expected<ExSubstituteRequest, ExFailure>>
substitute_command(std::string_view text)
{
    if (text.empty())
    {
        return std::nullopt;
    }
    if (text.front() == '&' || text.front() == '~')
    {
        const ExSubstituteSource source =
            text.front() == '&' ? ExSubstituteSource::repeated : ExSubstituteSource::tilde;
        ExSubstituteRequest request{std::nullopt, source, '/', {}, {}, false, {}, std::nullopt};
        return flags_read(skipped_white(text.substr(1)), std::move(request));
    }
    std::size_t name = 0;
    while (name < text.size() && is_letter(text[name]))
    {
        ++name;
    }
    if (flagged_s(text))
    {
        return substitute_argument(text.substr(1));
    }
    if (name == 0 || !substitute_name.starts_with(text.substr(0, name)))
    {
        return std::nullopt;
    }
    return substitute_argument(skipped_white(text.substr(name)));
}
} // namespace nenenib::core
