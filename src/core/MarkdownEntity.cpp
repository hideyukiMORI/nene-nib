#include "MarkdownEntity.hpp"

#include "Utf8.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr char32_t replacement_character = U'�';
constexpr char32_t last_code_point = 0x10FFFFU;
constexpr char32_t first_surrogate = 0xD800U;
constexpr char32_t last_surrogate = 0xDFFFU;

// 名前つき実体の表。CommonMark 0.31 の例（6.2 Entity and numeric character references と、
// リンク・コードの例）に出る名前だけ（ADR 0151 の決定 7・HTML5 の全表は入れない）。
constexpr std::array<std::pair<std::string_view, std::u32string_view>, 15> named_entities{{
    {"AElig", U"Æ"},
    {"ClockwiseContourIntegral", U"∲"},
    {"Dcaron", U"Ď"},
    {"DifferentialD", U"ⅆ"},
    {"HilbertSpace", U"ℋ"},
    {"amp", U"&"},
    {"auml", U"ä"},
    {"copy", U"©"},
    {"frac34", U"¾"},
    {"gt", U">"},
    {"lt", U"<"},
    {"nbsp", U" "},
    {"ngE", U"≧̸"},
    {"ouml", U"ö"},
    {"quot", U"\""},
}};

[[nodiscard]] std::optional<char32_t> digit_value(char digit, char32_t radix) noexcept
{
    if (digit >= '0' && digit <= '9')
    {
        return static_cast<char32_t>(digit - '0');
    }
    if (radix == 16U && digit >= 'a' && digit <= 'f')
    {
        return static_cast<char32_t>(digit - 'a' + 10);
    }
    if (radix == 16U && digit >= 'A' && digit <= 'F')
    {
        return static_cast<char32_t>(digit - 'A' + 10);
    }
    return std::nullopt;
}

// 数字の列を読む。U+10FFFF を超えたら超えた値で止める（それ以上は読まずに U+FFFD になる）。
[[nodiscard]] std::optional<char32_t> numeric_value(std::string_view digits,
                                                    char32_t radix) noexcept
{
    if (digits.empty())
    {
        return std::nullopt;
    }
    char32_t value = 0;
    for (const char digit : digits)
    {
        const auto next = digit_value(digit, radix);
        if (!next.has_value())
        {
            return std::nullopt;
        }
        if (value <= last_code_point)
        {
            value = (value * radix) + next.value();
        }
    }
    return value;
}

[[nodiscard]] char32_t shown_code_point(char32_t value) noexcept
{
    if (value == 0U || value > last_code_point ||
        (value >= first_surrogate && value <= last_surrogate))
    {
        return replacement_character;
    }
    return value;
}

[[nodiscard]] std::optional<std::u32string_view> named_value(std::string_view name) noexcept
{
    for (const auto &[entity, value] : named_entities)
    {
        if (entity == name)
        {
            return value;
        }
    }
    return std::nullopt;
}

// "&" と ";" の間。数値は "#" と "#x" / "#X" の後ろ。
[[nodiscard]] std::optional<std::u32string> entity_code_points(std::string_view body)
{
    if (!body.starts_with('#'))
    {
        return named_value(body).transform([](std::u32string_view value)
                                           { return std::u32string(value); });
    }
    const bool hexadecimal = body.starts_with("#x") || body.starts_with("#X");
    const auto value = numeric_value(body.substr(hexadecimal ? 2U : 1U), hexadecimal ? 16U : 10U);
    return value.transform([](char32_t code_point)
                           { return std::u32string(1U, shown_code_point(code_point)); });
}
} // namespace

std::string markdown_entity_text(std::string_view spelling)
{
    if (spelling.size() < 3U || !spelling.starts_with('&') || !spelling.ends_with(';'))
    {
        return std::string(spelling);
    }
    const auto code_points = entity_code_points(spelling.substr(1U, spelling.size() - 2U));
    if (!code_points.has_value())
    {
        return std::string(spelling);
    }
    std::string text;
    for (const char32_t code_point : code_points.value())
    {
        append_utf8(text, code_point);
    }
    return text;
}
} // namespace nenenib::core
