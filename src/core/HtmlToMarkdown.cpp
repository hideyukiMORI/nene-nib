#include "HtmlToMarkdown.hpp"

#include "HtmlAttribute.hpp"
#include "HtmlMarkdownWriter.hpp"
#include "HtmlTag.hpp"
#include "Utf8.hpp"

#include <array>
#include <optional>
#include <utility>

namespace nenenib::core
{
namespace
{
// 要素の名前（小文字）→ 値の表（決定 6・CPP-012）。表に無い名前は other（中身の文字だけ）。
constexpr std::array<std::pair<std::string_view, HtmlTag>, 51> tag_names{{
    {"a", HtmlTag::anchor},
    {"address", HtmlTag::division},
    {"area", HtmlTag::void_other},
    {"article", HtmlTag::division},
    {"aside", HtmlTag::division},
    {"b", HtmlTag::strong},
    {"base", HtmlTag::void_other},
    {"blockquote", HtmlTag::quote},
    {"br", HtmlTag::line_break},
    {"caption", HtmlTag::division},
    {"code", HtmlTag::code},
    {"col", HtmlTag::void_other},
    {"dd", HtmlTag::division},
    {"div", HtmlTag::division},
    {"dl", HtmlTag::division},
    {"dt", HtmlTag::division},
    {"em", HtmlTag::emphasis},
    {"embed", HtmlTag::void_other},
    {"figcaption", HtmlTag::division},
    {"figure", HtmlTag::division},
    {"footer", HtmlTag::division},
    {"h1", HtmlTag::heading_1},
    {"h2", HtmlTag::heading_2},
    {"h3", HtmlTag::heading_3},
    {"h4", HtmlTag::heading_4},
    {"h5", HtmlTag::heading_5},
    {"h6", HtmlTag::heading_6},
    {"header", HtmlTag::division},
    {"hr", HtmlTag::rule},
    {"i", HtmlTag::emphasis},
    {"img", HtmlTag::image},
    {"input", HtmlTag::void_other},
    {"li", HtmlTag::list_item},
    {"link", HtmlTag::void_other},
    {"main", HtmlTag::division},
    {"meta", HtmlTag::void_other},
    {"nav", HtmlTag::division},
    {"ol", HtmlTag::ordered_list},
    {"p", HtmlTag::paragraph},
    {"param", HtmlTag::void_other},
    {"pre", HtmlTag::preformatted},
    {"section", HtmlTag::division},
    {"source", HtmlTag::void_other},
    {"strong", HtmlTag::strong},
    {"table", HtmlTag::table},
    {"td", HtmlTag::table_cell},
    {"th", HtmlTag::table_cell},
    {"tr", HtmlTag::table_row},
    {"track", HtmlTag::void_other},
    {"ul", HtmlTag::unordered_list},
    {"wbr", HtmlTag::void_other},
}};

// 中身を文字として読まずに捨てる要素（決定 6）。
constexpr std::array<std::string_view, 2> discarded_elements{{"script", "style"}};

[[nodiscard]] constexpr bool is_letter(char character) noexcept
{
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

[[nodiscard]] constexpr bool is_name_character(char character) noexcept
{
    return is_letter(character) || (character >= '0' && character <= '9') || character == '-' ||
           character == ':';
}

[[nodiscard]] constexpr char lowered(char character) noexcept
{
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a')
                                                : character;
}

[[nodiscard]] std::string lowered_name(std::string_view name)
{
    std::string lower(name);
    for (char &character : lower)
    {
        character = lowered(character);
    }
    return lower;
}

[[nodiscard]] HtmlTag tag_of(std::string_view lower) noexcept
{
    for (const auto &[name, tag] : tag_names)
    {
        if (name == lower)
        {
            return tag;
        }
    }
    return HtmlTag::other;
}

[[nodiscard]] bool is_discarded(std::string_view lower) noexcept
{
    for (const std::string_view name : discarded_elements)
    {
        if (name == lower)
        {
            return true;
        }
    }
    return false;
}

// `<` が印の始まりか（`<a` `</a` `<!` `<?`）。ほかの `<`（`a < b`）は文字。
[[nodiscard]] bool starts_markup(std::string_view html, std::size_t at) noexcept
{
    const std::string_view rest = html.substr(at);
    if (rest.size() < 2U || rest[0] != '<')
    {
        return false;
    }
    if (rest[1] == '/')
    {
        return rest.size() > 2U && is_letter(rest[2]);
    }
    return is_letter(rest[1]) || rest[1] == '!' || rest[1] == '?';
}

// 文字列 needle の最初の位置。1 文字目を 1 バイトの探索で探し、残りを比べる（core の外へ部分列の
// 探索のシンボルを出さない・ARC-003）。無ければ npos。
[[nodiscard]] std::size_t text_at(std::string_view html, std::string_view needle,
                                  std::size_t from) noexcept
{
    for (std::size_t found = html.find(needle.front(), from); found != std::string_view::npos;
         found = html.find(needle.front(), found + 1U))
    {
        if (html.substr(found).starts_with(needle))
        {
            return found;
        }
    }
    return std::string_view::npos;
}

[[nodiscard]] std::size_t next_markup(std::string_view html, std::size_t at) noexcept
{
    std::size_t found = html.find('<', at);
    while (found != std::string_view::npos && !starts_markup(html, found))
    {
        found = html.find('<', found + 1U);
    }
    return found == std::string_view::npos ? html.size() : found;
}

[[nodiscard]] std::size_t name_end(std::string_view html, std::size_t at) noexcept
{
    while (at < html.size() && is_name_character(html[at]))
    {
        ++at;
    }
    return at;
}

// 引用符の中か（開いた引用符の文字・外なら NUL）を 1 文字ぶん進める。
[[nodiscard]] constexpr char quote_after(char quote, char character) noexcept
{
    if (quote != '\0')
    {
        return character == quote ? '\0' : quote;
    }
    return character == '"' || character == '\'' ? character : '\0';
}

// タグの `>` の位置。引用符の中の `>` は数えない。閉じなければ値なし（malformed）。
[[nodiscard]] std::optional<std::size_t> tag_end(std::string_view html, std::size_t at) noexcept
{
    char quote = '\0';
    for (; at < html.size(); ++at)
    {
        const char before = quote;
        quote = quote_after(quote, html[at]);
        if (before == '\0' && html[at] == '>')
        {
            return at;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> after(std::size_t found, std::size_t length) noexcept
{
    if (found == std::string_view::npos)
    {
        return std::nullopt;
    }
    return found + length;
}

// 捨てる要素の閉じタグの後ろ。閉じなければ終わりまで捨てる。
[[nodiscard]] std::size_t discarded_end(std::string_view html, std::size_t at,
                                        std::string_view lower)
{
    for (std::size_t found = text_at(html, "</", at); found != std::string_view::npos;
         found = text_at(html, "</", found + 2U))
    {
        const std::size_t end = name_end(html, found + 2U);
        if (lowered_name(html.substr(found + 2U, end - found - 2U)) != lower)
        {
            continue;
        }
        const auto close = tag_end(html, end);
        return close.has_value() ? close.value() + 1U : html.size();
    }
    return html.size();
}

[[nodiscard]] std::optional<std::size_t> read_end_tag(std::string_view html, std::size_t at,
                                                      HtmlMarkdownWriter &writer)
{
    const std::size_t end = name_end(html, at + 2U);
    const auto close = tag_end(html, end);
    if (!close.has_value())
    {
        return std::nullopt;
    }
    writer.close(tag_of(lowered_name(html.substr(at + 2U, end - at - 2U))));
    return close.value() + 1U;
}

// `<br/>` `<img src="x" />` の終わりの `/`。引用符の無い値の最後の `/`（`href=http://x/`）は値の
// 一部で、自閉じにしない。
[[nodiscard]] bool self_closed(std::string_view attributes) noexcept
{
    if (!attributes.ends_with('/'))
    {
        return false;
    }
    if (attributes.size() == 1U)
    {
        return true;
    }
    const char before = attributes[attributes.size() - 2U];
    return before == ' ' || before == '"' || before == '\'' || before == '\t' || before == '\n' ||
           before == '\r';
}

[[nodiscard]] std::optional<std::size_t> read_start_tag(std::string_view html, std::size_t at,
                                                        HtmlMarkdownWriter &writer)
{
    const std::size_t end = name_end(html, at + 1U);
    const auto close = tag_end(html, end);
    if (!close.has_value())
    {
        return std::nullopt;
    }
    const std::string lower = lowered_name(html.substr(at + 1U, end - at - 1U));
    if (is_discarded(lower))
    {
        return discarded_end(html, close.value() + 1U, lower);
    }
    std::string_view attributes = html.substr(end, close.value() - end);
    const bool self_closing = self_closed(attributes);
    attributes.remove_suffix(self_closing ? 1U : 0U);
    const HtmlTag tag = tag_of(lower);
    if (!writer.open(tag, attributes))
    {
        return std::nullopt;
    }
    if (self_closing)
    {
        writer.close(tag);
    }
    return close.value() + 1U;
}

// 位置 at の `<` から印 1 つを読み、次の位置を返す。終わらない印は値なし（決定 7 の malformed）。
[[nodiscard]] std::optional<std::size_t> read_markup(std::string_view html, std::size_t at,
                                                     HtmlMarkdownWriter &writer)
{
    const std::string_view rest = html.substr(at);
    if (rest.starts_with("<!--"))
    {
        return after(text_at(html, "-->", at + 4U), 3U);
    }
    if (rest.starts_with("<!") || rest.starts_with("<?"))
    {
        return after(html.find('>', at), 1U);
    }
    if (rest.starts_with("</"))
    {
        return read_end_tag(html, at, writer);
    }
    return read_start_tag(html, at, writer);
}
} // namespace

std::expected<std::string, HtmlFailure> markdown_from_html(std::string_view fragment)
{
    if (fragment.size() > html_fragment_limit)
    {
        return std::unexpected(HtmlFailure::too_large);
    }
    if (!validate_utf8(fragment).has_value())
    {
        return std::unexpected(HtmlFailure::malformed);
    }
    HtmlMarkdownWriter writer;
    std::size_t at = 0;
    while (at < fragment.size())
    {
        if (writer.size() > html_markdown_limit)
        {
            return std::unexpected(HtmlFailure::too_large);
        }
        const std::size_t markup = next_markup(fragment, at);
        writer.text(html_decoded(fragment.substr(at, markup - at)));
        if (markup == fragment.size())
        {
            break;
        }
        const auto next = read_markup(fragment, markup, writer);
        if (!next.has_value())
        {
            return std::unexpected(HtmlFailure::malformed);
        }
        at = next.value();
    }
    if (writer.size() > html_markdown_limit)
    {
        return std::unexpected(HtmlFailure::too_large);
    }
    return std::move(writer).finished();
}
} // namespace nenenib::core
