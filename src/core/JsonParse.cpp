#include "JsonParse.hpp"

#include "JsonFailure.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace nenenib::core
{
namespace
{
using Parsed = std::expected<JsonValue, JsonFailure>;
using ParsedText = std::expected<std::string, JsonFailure>;
using ParsedUnit = std::expected<char32_t, JsonFailure>;
using ParsedMember = std::expected<std::pair<std::string, JsonValue>, JsonFailure>;
// 区切りの後ろが閉じ括弧なら true、`,` なら false。
using ParsedClose = std::expected<bool, JsonFailure>;
using Step = std::expected<void, JsonFailure>;

// `\` の後ろの 1 文字と、それが表す文字（RFC 8259 の 7 節）。同じ添字が対になる。
constexpr std::string_view escape_names = "\"\\/bfnrt";
constexpr std::string_view escape_meanings = "\"\\/\b\f\n\r\t";
constexpr char32_t high_surrogate_first = 0xD800;
constexpr char32_t low_surrogate_first = 0xDC00;
constexpr char32_t surrogate_end = 0xE000;
constexpr char32_t supplementary_first = 0x10000;
constexpr unsigned surrogate_bits = 10U;
constexpr unsigned char first_printable = 0x20;

[[nodiscard]] bool is_space(char byte) noexcept
{
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r';
}

[[nodiscard]] bool is_digit(char byte) noexcept
{
    return byte >= '0' && byte <= '9';
}

[[nodiscard]] bool at_byte(std::string_view text, std::size_t at, char byte) noexcept
{
    return at < text.size() && text[at] == byte;
}

void skip_space(std::string_view text, std::size_t &at) noexcept
{
    while (at < text.size() && is_space(text[at]))
    {
        ++at;
    }
}

// 1 つ以上の数字を読む。1 つも無ければ偽。
[[nodiscard]] bool digits(std::string_view text, std::size_t &at) noexcept
{
    const std::size_t begin = at;
    while (at < text.size() && is_digit(text[at]))
    {
        ++at;
    }
    return at != begin;
}

// 整数部は `0` か `[1-9][0-9]*`。`01` は断る。
[[nodiscard]] bool integer_part(std::string_view text, std::size_t &at) noexcept
{
    if (at_byte(text, at, '0'))
    {
        ++at;
        return !(at < text.size() && is_digit(text[at]));
    }
    return digits(text, at);
}

[[nodiscard]] bool fraction_part(std::string_view text, std::size_t &at) noexcept
{
    if (!at_byte(text, at, '.'))
    {
        return true;
    }
    ++at;
    return digits(text, at);
}

[[nodiscard]] bool exponent_part(std::string_view text, std::size_t &at) noexcept
{
    if (!at_byte(text, at, 'e') && !at_byte(text, at, 'E'))
    {
        return true;
    }
    ++at;
    if (at_byte(text, at, '+') || at_byte(text, at, '-'))
    {
        ++at;
    }
    return digits(text, at);
}

// 数の文法を先に確かめ、値は std::from_chars で読む（ロケールに依らない・ARC-007）。
[[nodiscard]] Parsed parse_number(std::string_view text, std::size_t &at)
{
    const std::size_t begin = at;
    if (at_byte(text, at, '-'))
    {
        ++at;
    }
    if (!integer_part(text, at) || !fraction_part(text, at) || !exponent_part(text, at))
    {
        return std::unexpected(JsonFailure::invalid_number);
    }
    const std::string_view written = text.substr(begin, at - begin);
    const char *const last = written.data() + written.size();
    double value = 0.0;
    const auto result = std::from_chars(written.data(), last, value);
    if (result.ec == std::errc::result_out_of_range)
    {
        return std::unexpected(JsonFailure::number_out_of_range);
    }
    if (result.ec != std::errc{} || result.ptr != last)
    {
        return std::unexpected(JsonFailure::invalid_number);
    }
    return JsonValue{json_number(value)};
}

[[nodiscard]] std::optional<char32_t> hex_digit(char byte) noexcept
{
    if (is_digit(byte))
    {
        return static_cast<char32_t>(byte - '0');
    }
    if (byte >= 'a' && byte <= 'f')
    {
        return static_cast<char32_t>(byte - 'a' + 10);
    }
    if (byte >= 'A' && byte <= 'F')
    {
        return static_cast<char32_t>(byte - 'A' + 10);
    }
    return std::nullopt;
}

// `\u` の後ろの 16 進 4 桁を UTF-16 の 1 単位として読む。
[[nodiscard]] ParsedUnit hex_unit(std::string_view text, std::size_t &at)
{
    char32_t unit = 0;
    for (std::size_t count = 0; count < 4; ++count)
    {
        if (at >= text.size())
        {
            return std::unexpected(JsonFailure::unexpected_end);
        }
        const auto digit = hex_digit(text[at]);
        if (!digit.has_value())
        {
            return std::unexpected(JsonFailure::invalid_unicode_escape);
        }
        unit = (unit << 4U) | digit.value();
        ++at;
    }
    return unit;
}

[[nodiscard]] bool is_high_surrogate(char32_t unit) noexcept
{
    return unit >= high_surrogate_first && unit < low_surrogate_first;
}

[[nodiscard]] bool is_low_surrogate(char32_t unit) noexcept
{
    return unit >= low_surrogate_first && unit < surrogate_end;
}

// `\uXXXX`（代理対なら `\uXXXX\uXXXX` の 2 つ）を 1 つのコードポイントにして UTF-8 で足す。
[[nodiscard]] Step unicode_escape(std::string_view text, std::size_t &at, std::string &out)
{
    const auto first = hex_unit(text, at);
    if (!first.has_value())
    {
        return std::unexpected(first.error());
    }
    const char32_t high = first.value();
    if (is_low_surrogate(high))
    {
        return std::unexpected(JsonFailure::unpaired_surrogate);
    }
    if (!is_high_surrogate(high))
    {
        append_utf8(out, high);
        return {};
    }
    if (text.substr(at, 2) != "\\u")
    {
        return std::unexpected(JsonFailure::unpaired_surrogate);
    }
    at += 2;
    const auto second = hex_unit(text, at);
    if (!second.has_value())
    {
        return std::unexpected(second.error());
    }
    if (!is_low_surrogate(second.value()))
    {
        return std::unexpected(JsonFailure::unpaired_surrogate);
    }
    append_utf8(out, supplementary_first + ((high - high_surrogate_first) << surrogate_bits) +
                         (second.value() - low_surrogate_first));
    return {};
}

// `\` の後ろを読む（at は `\` の次）。
[[nodiscard]] Step escape(std::string_view text, std::size_t &at, std::string &out)
{
    if (at >= text.size())
    {
        return std::unexpected(JsonFailure::unexpected_end);
    }
    const char name = text[at];
    ++at;
    // string_view::find(char) は memchr を core の外へ出すので、ranges::find で引く（ARC-003）。
    const auto known = std::ranges::find(escape_names, name);
    if (known != escape_names.end())
    {
        out.push_back(escape_meanings[static_cast<std::size_t>(known - escape_names.begin())]);
        return {};
    }
    if (name != 'u')
    {
        return std::unexpected(JsonFailure::invalid_escape);
    }
    return unicode_escape(text, at, out);
}

// 文字列の中身を読む（at は開きの `"` の次・閉じの `"` の次まで進める）。入力全体は検証済みの
// UTF-8 なので、生のバイトはそのまま写す。
[[nodiscard]] ParsedText parse_string(std::string_view text, std::size_t &at)
{
    std::string out;
    while (at < text.size())
    {
        const char byte = text[at];
        ++at;
        if (byte == '"')
        {
            return out;
        }
        if (static_cast<unsigned char>(byte) < first_printable)
        {
            return std::unexpected(JsonFailure::control_character);
        }
        if (byte != '\\')
        {
            out.push_back(byte);
            continue;
        }
        const Step step = escape(text, at, out);
        if (!step.has_value())
        {
            return std::unexpected(step.error());
        }
    }
    return std::unexpected(JsonFailure::unexpected_end);
}

// 空白を飛ばして byte を 1 つ読む。
[[nodiscard]] Step expect_byte(std::string_view text, std::size_t &at, char byte)
{
    skip_space(text, at);
    if (at >= text.size())
    {
        return std::unexpected(JsonFailure::unexpected_end);
    }
    if (text[at] != byte)
    {
        return std::unexpected(JsonFailure::unexpected_character);
    }
    ++at;
    return {};
}

// 配列や object の要素の後ろの `,` か閉じ括弧を読む。
[[nodiscard]] ParsedClose separator(std::string_view text, std::size_t &at, char close)
{
    skip_space(text, at);
    if (at >= text.size())
    {
        return std::unexpected(JsonFailure::unexpected_end);
    }
    const char byte = text[at];
    ++at;
    if (byte == ',')
    {
        return false;
    }
    if (byte == close)
    {
        return true;
    }
    return std::unexpected(JsonFailure::unexpected_character);
}

// 空でない並びかを確かめて、空なら閉じ括弧を読む。
[[nodiscard]] bool empty_list(std::string_view text, std::size_t &at, char close) noexcept
{
    skip_space(text, at);
    if (!at_byte(text, at, close))
    {
        return false;
    }
    ++at;
    return true;
}

[[nodiscard]] Parsed parse_value(std::string_view text, std::size_t &at, std::size_t depth);

[[nodiscard]] Parsed parse_array(std::string_view text, std::size_t &at, std::size_t depth)
{
    if (depth >= json_depth_limit)
    {
        return std::unexpected(JsonFailure::too_deep);
    }
    ++at;
    JsonArray items;
    if (empty_list(text, at, ']'))
    {
        return JsonValue{std::move(items)};
    }
    while (true)
    {
        auto item = parse_value(text, at, depth + 1);
        if (!item.has_value())
        {
            return item;
        }
        items.push_back(std::move(item).value());
        const ParsedClose closed = separator(text, at, ']');
        if (!closed.has_value())
        {
            return std::unexpected(closed.error());
        }
        if (closed.value())
        {
            return JsonValue{std::move(items)};
        }
    }
}

[[nodiscard]] ParsedMember parse_member(std::string_view text, std::size_t &at, std::size_t depth)
{
    const Step opened = expect_byte(text, at, '"');
    if (!opened.has_value())
    {
        return std::unexpected(opened.error());
    }
    auto key = parse_string(text, at);
    if (!key.has_value())
    {
        return std::unexpected(key.error());
    }
    const Step colon = expect_byte(text, at, ':');
    if (!colon.has_value())
    {
        return std::unexpected(colon.error());
    }
    auto value = parse_value(text, at, depth);
    if (!value.has_value())
    {
        return std::unexpected(value.error());
    }
    return std::pair{std::move(key).value(), std::move(value).value()};
}

[[nodiscard]] Parsed parse_object(std::string_view text, std::size_t &at, std::size_t depth)
{
    if (depth >= json_depth_limit)
    {
        return std::unexpected(JsonFailure::too_deep);
    }
    ++at;
    JsonObject members;
    if (empty_list(text, at, '}'))
    {
        return JsonValue{std::move(members)};
    }
    while (true)
    {
        auto member = parse_member(text, at, depth + 1);
        if (!member.has_value())
        {
            return std::unexpected(member.error());
        }
        members.push_back(std::move(member).value());
        const ParsedClose closed = separator(text, at, '}');
        if (!closed.has_value())
        {
            return std::unexpected(closed.error());
        }
        if (closed.value())
        {
            return JsonValue{std::move(members)};
        }
    }
}

[[nodiscard]] bool literal_at(std::string_view text, std::size_t &at, std::string_view word)
{
    if (text.substr(at, word.size()) != word)
    {
        return false;
    }
    at += word.size();
    return true;
}

[[nodiscard]] Parsed parse_literal(std::string_view text, std::size_t &at)
{
    if (literal_at(text, at, "true"))
    {
        return JsonValue{true};
    }
    if (literal_at(text, at, "false"))
    {
        return JsonValue{false};
    }
    if (literal_at(text, at, "null"))
    {
        return JsonValue{JsonNull{}};
    }
    const char byte = text[at];
    if (byte == 't' || byte == 'f' || byte == 'n')
    {
        return std::unexpected(JsonFailure::invalid_literal);
    }
    return std::unexpected(JsonFailure::unexpected_character);
}

Parsed parse_value(std::string_view text, std::size_t &at, std::size_t depth)
{
    skip_space(text, at);
    if (at >= text.size())
    {
        return std::unexpected(JsonFailure::unexpected_end);
    }
    const char byte = text[at];
    if (byte == '{')
    {
        return parse_object(text, at, depth);
    }
    if (byte == '[')
    {
        return parse_array(text, at, depth);
    }
    if (byte == '"')
    {
        ++at;
        return parse_string(text, at).transform([](std::string value)
                                                { return JsonValue{std::move(value)}; });
    }
    if (byte == '-' || is_digit(byte))
    {
        return parse_number(text, at);
    }
    return parse_literal(text, at);
}
} // namespace

std::expected<JsonValue, JsonFailure> parse_json(std::string_view text)
{
    if (!validate_utf8(text).has_value())
    {
        return std::unexpected(JsonFailure::invalid_utf8);
    }
    std::size_t at = 0;
    auto value = parse_value(text, at, 0);
    if (!value.has_value())
    {
        return value;
    }
    skip_space(text, at);
    if (at != text.size())
    {
        return std::unexpected(JsonFailure::trailing_characters);
    }
    return value;
}
} // namespace nenenib::core
