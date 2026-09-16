#include "Utf8.hpp"

#include <algorithm>
#include <array>

namespace nenenib::core
{
namespace
{
constexpr char32_t first_surrogate = 0xD800;
constexpr char32_t last_surrogate = 0xDFFF;
constexpr char32_t last_code_point = 0x10FFFF;
constexpr unsigned char first_control = 0x20;
constexpr unsigned char delete_character = 0x7F;
constexpr char32_t first_supplementary = 0x10000;
constexpr unsigned int continuation_shift = 6U;
constexpr char32_t continuation_mask = 0x3F;

// UTF-8 の継続バイト（10xxxxxx）を 1 つ作る。
[[nodiscard]] constexpr char continuation_byte(char32_t value, unsigned int shift) noexcept
{
    return static_cast<char>(0x80U | ((value >> shift) & continuation_mask));
}

[[nodiscard]] constexpr char lead_byte(char32_t value, unsigned int shift,
                                       unsigned int marker) noexcept
{
    return static_cast<char>(marker | (value >> shift));
}

[[nodiscard]] constexpr unsigned char byte_at(std::string_view text, std::size_t index) noexcept
{
    return static_cast<unsigned char>(text[index]);
}

[[nodiscard]] constexpr std::size_t sequence_length(unsigned char lead) noexcept
{
    if (lead < 0x80U)
    {
        return 1;
    }
    if (lead >= 0xC0U && lead < 0xE0U)
    {
        return 2;
    }
    if (lead >= 0xE0U && lead < 0xF0U)
    {
        return 3;
    }
    if (lead >= 0xF0U && lead < 0xF8U)
    {
        return 4;
    }
    // 0x80-0xBF は孤立した継続バイト、0xF8 以降は UTF-8 に存在しない先頭バイト。
    return 0;
}

[[nodiscard]] constexpr char32_t lead_value(unsigned char lead, std::size_t length) noexcept
{
    constexpr std::array<unsigned char, 5> mask{0U, 0x7FU, 0x1FU, 0x0FU, 0x07U};
    return static_cast<char32_t>(lead & mask.at(length));
}

[[nodiscard]] constexpr bool continuation(unsigned char byte) noexcept
{
    return (byte & 0xC0U) == 0x80U;
}

// 過長符号化・サロゲート範囲・U+10FFFF 超をまとめて拒否する。
[[nodiscard]] constexpr bool encodable(char32_t value, std::size_t length) noexcept
{
    constexpr std::array<char32_t, 5> smallest{0U, 0U, 0x80U, 0x800U, 0x10000U};
    return value >= smallest.at(length) && value <= last_code_point &&
           (value < first_surrogate || value > last_surrogate);
}

// 長さの分かっている列を値へ。継続バイトの形は呼び出し側が確かめてから来る。
[[nodiscard]] constexpr char32_t decode(std::string_view text, std::size_t index,
                                        std::size_t length) noexcept
{
    char32_t value = lead_value(byte_at(text, index), length);
    for (std::size_t offset = 1; offset < length; ++offset)
    {
        value = (value << 6U) | static_cast<char32_t>(byte_at(text, index + offset) & 0x3FU);
    }
    return value;
}

[[nodiscard]] std::expected<std::size_t, TextFailure> scan(std::string_view text,
                                                           std::size_t index) noexcept
{
    const unsigned char lead = byte_at(text, index);
    const std::size_t length = sequence_length(lead);
    if (length == 0 || text.size() - index < length)
    {
        return std::unexpected(TextFailure::invalid_utf8);
    }
    for (std::size_t offset = 1; offset < length; ++offset)
    {
        if (!continuation(byte_at(text, index + offset)))
        {
            return std::unexpected(TextFailure::invalid_utf8);
        }
    }
    if (!encodable(decode(text, index, length), length))
    {
        return std::unexpected(TextFailure::invalid_utf8);
    }
    return length;
}
} // namespace

// 閉じた選択肢ではなく値の範囲なので早期 return で並べる（CPP-002 の else 禁止は掛からない）。
void append_utf8(std::string &utf8, char32_t value)
{
    if (value < 0x80U)
    {
        utf8.push_back(static_cast<char>(value));
        return;
    }
    if (value < 0x800U)
    {
        utf8.push_back(lead_byte(value, continuation_shift, 0xC0U));
        utf8.push_back(continuation_byte(value, 0U));
        return;
    }
    if (value < first_supplementary)
    {
        utf8.push_back(lead_byte(value, 2U * continuation_shift, 0xE0U));
        utf8.push_back(continuation_byte(value, continuation_shift));
        utf8.push_back(continuation_byte(value, 0U));
        return;
    }
    utf8.push_back(lead_byte(value, 3U * continuation_shift, 0xF0U));
    utf8.push_back(continuation_byte(value, 2U * continuation_shift));
    utf8.push_back(continuation_byte(value, continuation_shift));
    utf8.push_back(continuation_byte(value, 0U));
}

std::expected<std::size_t, TextFailure> validate_utf8(std::string_view text) noexcept
{
    std::size_t index = 0;
    std::size_t code_points = 0;
    while (index < text.size())
    {
        const auto length = scan(text, index);
        if (!length)
        {
            return std::unexpected(length.error());
        }
        index += length.value();
        ++code_points;
    }
    return code_points;
}

std::size_t code_point_count(std::string_view text) noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(
        text, [](char value) { return !continuation(static_cast<unsigned char>(value)); }));
}

bool has_control_character(std::string_view text) noexcept
{
    return std::ranges::any_of(text,
                               [](char value)
                               {
                                   const auto byte = static_cast<unsigned char>(value);
                                   return byte < first_control || byte == delete_character;
                               });
}

char32_t code_point_at(std::string_view text, Offset at) noexcept
{
    if (at.value >= text.size())
    {
        return 0;
    }
    return decode(text, at.value, sequence_length(byte_at(text, at.value)));
}

bool is_boundary(std::string_view text, Offset at) noexcept
{
    return at.value >= text.size() || !continuation(byte_at(text, at.value));
}

Offset next_code_point(std::string_view text, Offset at) noexcept
{
    std::size_t index = std::min(at.value, text.size());
    if (index < text.size())
    {
        ++index;
    }
    while (index < text.size() && continuation(byte_at(text, index)))
    {
        ++index;
    }
    return Offset{index};
}

Offset previous_code_point(std::string_view text, Offset at) noexcept
{
    std::size_t index = std::min(at.value, text.size());
    if (index > 0)
    {
        --index;
    }
    while (index > 0 && continuation(byte_at(text, index)))
    {
        --index;
    }
    return Offset{index};
}
} // namespace nenenib::core
