#include "HtmlAttribute.hpp"

#include "MarkdownEntity.hpp"

#include <cstddef>
#include <optional>
#include <tuple>

namespace nenenib::core
{
namespace
{
// 実体参照の綴りの長さの上限（`&ClockwiseContourIntegral;` が収まる）。これより遠い `;` は見ない。
constexpr std::size_t entity_reach = 32;

// 属性 1 つの読み: 書かれた名前・値（無ければ値なし）・次の位置。
using AttributeRead = std::tuple<std::string_view, std::optional<std::string_view>, std::size_t>;

[[nodiscard]] constexpr bool is_space(char character) noexcept
{
    return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
           character == '\f';
}

[[nodiscard]] constexpr char lowered(char character) noexcept
{
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a')
                                                : character;
}

[[nodiscard]] bool same_name(std::string_view written, std::string_view name) noexcept
{
    if (written.size() != name.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < written.size(); ++index)
    {
        if (lowered(written[index]) != name[index])
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::size_t skipped_spaces(std::string_view text, std::size_t at) noexcept
{
    while (at < text.size() && is_space(text[at]))
    {
        ++at;
    }
    return at;
}

// 属性の名前の終わり（空白・`=`・`/`・`>` の手前）。
[[nodiscard]] std::size_t name_end(std::string_view text, std::size_t at) noexcept
{
    while (at < text.size() && !is_space(text[at]) && text[at] != '=' && text[at] != '/' &&
           text[at] != '>')
    {
        ++at;
    }
    return at;
}

// 値の終わり。引用符つきは閉じる引用符の位置（無ければ終わり）。
[[nodiscard]] std::size_t value_end(std::string_view text, std::size_t begin, char quote) noexcept
{
    if (quote != '\0')
    {
        const std::size_t closing = text.find(quote, begin);
        return closing == std::string_view::npos ? text.size() : closing;
    }
    std::size_t at = begin;
    while (at < text.size() && !is_space(text[at]) && text[at] != '>')
    {
        ++at;
    }
    return at;
}

[[nodiscard]] char quote_at(std::string_view text, std::size_t at) noexcept
{
    if (at < text.size() && (text[at] == '"' || text[at] == '\''))
    {
        return text[at];
    }
    return '\0';
}

// 位置 at から属性 1 つ: 書かれた名前・値（`=` が無ければ値なし）・次の属性の位置。
[[nodiscard]] AttributeRead attribute_at(std::string_view text, std::size_t at) noexcept
{
    const std::size_t end = name_end(text, at);
    const std::string_view written = text.substr(at, end - at);
    std::size_t next = skipped_spaces(text, end);
    if (next >= text.size() || text[next] != '=')
    {
        // 名前になれない文字（`/` など）は 1 つ飛ばす。
        return {written, std::nullopt, written.empty() ? next + 1U : next};
    }
    next = skipped_spaces(text, next + 1U);
    const char quote = quote_at(text, next);
    const std::size_t begin = quote == '\0' ? next : next + 1U;
    const std::size_t finish = value_end(text, begin, quote);
    return {written, text.substr(begin, finish - begin),
            skipped_spaces(text, quote == '\0' ? finish : finish + 1U)};
}

[[nodiscard]] std::string_view entity_at(std::string_view text, std::size_t at) noexcept
{
    const std::size_t semicolon = text.substr(at, entity_reach).find(';');
    return semicolon == std::string_view::npos ? std::string_view{}
                                               : text.substr(at, semicolon + 1U);
}
} // namespace

std::optional<std::string> html_attribute(std::string_view attributes, std::string_view name)
{
    std::size_t at = skipped_spaces(attributes, 0);
    while (at < attributes.size())
    {
        const auto [written, value, next] = attribute_at(attributes, at);
        if (same_name(written, name))
        {
            return value.transform(html_decoded).value_or(std::string{});
        }
        at = next;
    }
    return std::nullopt;
}

std::string html_decoded(std::string_view text)
{
    std::string decoded;
    decoded.reserve(text.size());
    std::size_t at = 0;
    while (at < text.size())
    {
        const std::size_t ampersand = text.find('&', at);
        if (ampersand == std::string_view::npos)
        {
            decoded += text.substr(at);
            break;
        }
        decoded += text.substr(at, ampersand - at);
        const std::string_view spelling = entity_at(text, ampersand);
        if (spelling.empty())
        {
            decoded += '&';
            at = ampersand + 1U;
            continue;
        }
        decoded += markdown_entity_text(spelling);
        at = ampersand + spelling.size();
    }
    return decoded;
}
} // namespace nenenib::core
