#include "UiTexts.hpp"

#include "UiTextRow.hpp"
#include "UiTextsEnglish.hpp"
#include "UiTextsJapanese.hpp"
#include "UiTextsSimplifiedChinese.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::size_t placeholder_size = 3; // `{` 数字 `}`

// text の at から始まる置き場 {0}〜{9} の番号。置き場の形でなければ値なし。
[[nodiscard]] constexpr std::optional<std::size_t> placeholder_at(std::string_view text,
                                                                  std::size_t at) noexcept
{
    if (at + placeholder_size > text.size() || text[at] != '{' || text[at + 2] != '}')
    {
        return std::nullopt;
    }
    const char digit = text[at + 1];
    if (digit < '0' || digit > '9')
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(digit - '0');
}

// 文が持つ置き場の番号の集合（ビット i が {i}）。
[[nodiscard]] consteval std::uint16_t placeholder_mask(std::string_view text) noexcept
{
    std::uint16_t mask = 0;
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        const auto slot = placeholder_at(text, at);
        if (slot.has_value())
        {
            mask = static_cast<std::uint16_t>(mask | (1U << slot.value()));
        }
    }
    return mask;
}

// UTF-8 の 1 文字の長さ（先頭のバイトから）。先頭になれないバイトは 0。
[[nodiscard]] consteval std::size_t sequence_size(unsigned char lead) noexcept
{
    if (lead < 0x80U)
    {
        return 1;
    }
    if (lead >= 0xC2U && lead <= 0xDFU)
    {
        return 2;
    }
    if (lead >= 0xE0U && lead <= 0xEFU)
    {
        return 3;
    }
    if (lead >= 0xF0U && lead <= 0xF4U)
    {
        return 4;
    }
    return 0;
}

// 2 バイト目の許される範囲（長すぎる形・サロゲート・U+10FFFF を超える値を断る）。
[[nodiscard]] consteval bool second_allowed(unsigned char lead, unsigned char second) noexcept
{
    const unsigned char low = lead == 0xE0U ? 0xA0U : lead == 0xF0U ? 0x90U : 0x80U;
    const unsigned char high = lead == 0xEDU ? 0x9FU : lead == 0xF4U ? 0x8FU : 0xBFU;
    return second >= low && second <= high;
}

// at から始まる 1 文字（size バイト）の続きのバイトが正しいか。
[[nodiscard]] consteval bool continuation_valid(std::string_view text, std::size_t at,
                                                std::size_t size) noexcept
{
    const auto lead = static_cast<unsigned char>(text[at]);
    for (std::size_t index = 1; index < size; ++index)
    {
        const auto next = static_cast<unsigned char>(text[at + index]);
        const bool allowed =
            index == 1 ? second_allowed(lead, next) : next >= 0x80U && next <= 0xBFU;
        if (!allowed)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] consteval bool valid_utf8(std::string_view text) noexcept
{
    std::size_t at = 0;
    while (at < text.size())
    {
        const std::size_t size = sequence_size(static_cast<unsigned char>(text[at]));
        if (size == 0 || at + size > text.size() || !continuation_valid(text, at, size))
        {
            return false;
        }
        at += size;
    }
    return true;
}

// 列の i 行目が i 番目の値を持ち（UiMessage の順に 1 回ずつ）、文が空でなく正しい UTF-8 か。
[[nodiscard]] consteval bool column_complete(const UiTextColumn &column) noexcept
{
    for (std::size_t index = 0; index < column.size(); ++index)
    {
        const UiTextRow &row = column[index];
        if (std::to_underlying(row.message) != index || row.text.empty() || !valid_utf8(row.text))
        {
            return false;
        }
    }
    return true;
}

// どの言語の文も、同じ値では同じ置き場の集合を持つ（決定 5）。
[[nodiscard]] consteval bool same_placeholders() noexcept
{
    for (std::size_t index = 0; index < ui_message_count; ++index)
    {
        const std::uint16_t japanese = placeholder_mask(ui_texts_japanese[index].text);
        if (placeholder_mask(ui_texts_english[index].text) != japanese ||
            placeholder_mask(ui_texts_simplified_chinese[index].text) != japanese)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const UiTextColumn &column_of(DisplayLanguage language) noexcept
{
    switch (language)
    {
    case DisplayLanguage::japanese:
        return ui_texts_japanese;
    case DisplayLanguage::english:
        return ui_texts_english;
    case DisplayLanguage::simplified_chinese:
        return ui_texts_simplified_chinese;
    }
    std::unreachable();
}
} // namespace

static_assert(column_complete(ui_texts_japanese),
              "ui_texts_japanese has one non-empty UTF-8 row for every UiMessage in order");
static_assert(column_complete(ui_texts_english),
              "ui_texts_english has one non-empty UTF-8 row for every UiMessage in order");
static_assert(
    column_complete(ui_texts_simplified_chinese),
    "ui_texts_simplified_chinese has one non-empty UTF-8 row for every UiMessage in order");
static_assert(same_placeholders(), "every language has the same placeholders for a UiMessage");

std::string_view ui_text(UiMessage message, DisplayLanguage language) noexcept
{
    return column_of(language)[std::to_underlying(message)].text;
}

std::string ui_text_filled(UiMessage message, DisplayLanguage language,
                           std::initializer_list<std::string_view> values)
{
    return placeholders_filled(ui_text(message, language), values);
}

std::string placeholders_filled(std::string_view text,
                                std::initializer_list<std::string_view> values)
{
    std::string filled;
    filled.reserve(text.size());
    std::size_t at = 0;
    while (at < text.size())
    {
        const auto slot = placeholder_at(text, at);
        if (slot.has_value() && slot.value() < values.size())
        {
            filled += *std::next(values.begin(), static_cast<std::ptrdiff_t>(slot.value()));
            at += placeholder_size;
            continue;
        }
        filled += text[at];
        ++at;
    }
    return filled;
}

std::pair<std::string_view, std::string_view> first_slot_split(std::string_view text) noexcept
{
    constexpr std::string_view slot = "{0}";
    for (std::size_t index = 0; index + slot.size() <= text.size(); ++index)
    {
        if (text[index] == slot[0] && text[index + 1] == slot[1] && text[index + 2] == slot[2])
        {
            return {text.substr(0, index), text.substr(index + slot.size())};
        }
    }
    return {text, {}};
}
} // namespace nenenib::core
