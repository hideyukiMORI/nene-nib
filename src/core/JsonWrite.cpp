#include "JsonWrite.hpp"

#include "JsonNull.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace nenenib::core
{
namespace
{
// double の最短の往復の形と int64 の 10 進は 24 文字に収まる。余裕を持たせる。
constexpr std::size_t number_capacity = 32;
constexpr unsigned char first_printable = 0x20;
constexpr std::string_view hex_digits = "0123456789abcdef";

void append_value(std::string &out, const JsonValue &value);

void append_alternative(std::string &out, JsonNull /*value*/)
{
    out.append("null");
}

void append_alternative(std::string &out, bool value)
{
    out.append(value ? "true" : "false");
}

void append_alternative(std::string &out, const JsonNumber &number)
{
    std::array<char, number_capacity> digits{};
    char *const first = digits.data();
    char *const last = first + digits.size();
    // 有限かは x - x が 0 かで見る（NaN と無限は NaN になる）。libm の isfinite
    // を呼ばない（ARC-003）。
    if (!number.integer.has_value() && number.value - number.value != 0.0)
    {
        out.append("null");
        return;
    }
    const auto result = number.integer.has_value()
                            ? std::to_chars(first, last, number.integer.value())
                            : std::to_chars(first, last, number.value);
    if (result.ec != std::errc{})
    {
        out.append("null");
        return;
    }
    out.append(first, result.ptr);
}

// 制御文字の escape。短い形のあるものはそれを、ほかは `\u00XX` を書く。
void append_control(std::string &out, unsigned char byte)
{
    constexpr std::string_view short_bytes = "\b\f\n\r\t";
    constexpr std::string_view short_names = "bfnrt";
    const auto known = std::ranges::find(short_bytes, static_cast<char>(byte));
    if (known != short_bytes.end())
    {
        out.push_back('\\');
        out.push_back(short_names[static_cast<std::size_t>(known - short_bytes.begin())]);
        return;
    }
    out.append("\\u00");
    out.push_back(hex_digits[byte >> 4U]);
    out.push_back(hex_digits[byte & 0x0FU]);
}

void append_alternative(std::string &out, const std::string &text)
{
    out.push_back('"');
    for (const char byte : text)
    {
        const auto unit = static_cast<unsigned char>(byte);
        if (unit < first_printable)
        {
            append_control(out, unit);
            continue;
        }
        if (byte == '"' || byte == '\\')
        {
            out.push_back('\\');
        }
        out.push_back(byte);
    }
    out.push_back('"');
}

void append_alternative(std::string &out, const JsonArray &items)
{
    out.push_back('[');
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        if (index != 0)
        {
            out.push_back(',');
        }
        append_value(out, items[index]);
    }
    out.push_back(']');
}

void append_alternative(std::string &out, const JsonObject &members)
{
    out.push_back('{');
    for (std::size_t index = 0; index < members.size(); ++index)
    {
        if (index != 0)
        {
            out.push_back(',');
        }
        append_alternative(out, members[index].first);
        out.push_back(':');
        append_value(out, members[index].second);
    }
    out.push_back('}');
}

void append_value(std::string &out, const JsonValue &value)
{
    std::visit([&out](const auto &alternative) { append_alternative(out, alternative); },
               value.value);
}
} // namespace

std::string write_json(const JsonValue &value)
{
    std::string out;
    append_value(out, value);
    return out;
}
} // namespace nenenib::core
