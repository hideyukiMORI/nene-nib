#pragma once

#include "EncodingFailure.hpp"

#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>

namespace nenenib::core
{
// 読み書きできる文字コードの閉じた集合（ADR 0010 の決定 3）。CP932 の表は持たず、
// 変換は adapters の CodePagePort が行う。ここにあるのは判別と表示の名前だけ。
enum class TextEncoding : std::uint8_t
{
    utf8,
    utf8_bom,
    shift_jis,
    // BOM つきの UTF-16 だけ（ADR 0149 の決定 1）。本文は読むときに UTF-8 へ写し、保存で戻す。
    utf16_le,
    utf16_be
};

// ステータスバーに出す名前（ADR 0010 の決定 14）。
[[nodiscard]] constexpr std::string_view encoding_label(TextEncoding encoding) noexcept
{
    switch (encoding)
    {
    case TextEncoding::utf8:
        return "UTF-8";
    case TextEncoding::utf8_bom:
        return "UTF-8 BOM";
    case TextEncoding::shift_jis:
        return "Shift_JIS";
    case TextEncoding::utf16_le:
        return "UTF-16 LE";
    case TextEncoding::utf16_be:
        return "UTF-16 BE";
    }
    std::unreachable();
}

// UTF-8 の BOM。本文には含めず、保存のときだけ前に付け直す（決定 5）。
[[nodiscard]] constexpr std::string_view byte_order_mark() noexcept
{
    return "\xEF\xBB\xBF";
}

// UTF-16 の BOM（ADR 0149 の決定 1）。LE は FF FE、BE は FE FF。
[[nodiscard]] constexpr std::string_view utf16_le_byte_order_mark() noexcept
{
    return "\xFF\xFE";
}

[[nodiscard]] constexpr std::string_view utf16_be_byte_order_mark() noexcept
{
    return "\xFE\xFF";
}

// UTF-32 の BOM（ADR 0149 の決定 1・#734）。読まずに断るための印で、LE は UTF-16 LE の BOM
// で始まる。
[[nodiscard]] constexpr std::string_view utf32_le_byte_order_mark() noexcept
{
    return std::string_view{"\xFF\xFE\x00\x00", 4};
}

[[nodiscard]] constexpr std::string_view utf32_be_byte_order_mark() noexcept
{
    return std::string_view{"\x00\x00\xFE\xFF", 4};
}

[[nodiscard]] constexpr std::string_view without_byte_order_mark(std::string_view bytes) noexcept
{
    if (bytes.starts_with(byte_order_mark()))
    {
        return bytes.substr(byte_order_mark().size());
    }
    return bytes;
}

// UTF-16 の BOM → UTF-8 の BOM → 正しい UTF-8 → CP932 として正しい、の順に見る。BOM の無い
// 列では UTF-8 が常に勝つ（決定 3・ADR 0149 の決定 1）。
[[nodiscard]] std::expected<TextEncoding, EncodingFailure>
detect_encoding(std::string_view bytes) noexcept;
} // namespace nenenib::core
