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
    shift_jis
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
    }
    std::unreachable();
}

// UTF-8 の BOM。本文には含めず、保存のときだけ前に付け直す（決定 5）。
[[nodiscard]] constexpr std::string_view byte_order_mark() noexcept
{
    return "\xEF\xBB\xBF";
}

[[nodiscard]] constexpr std::string_view without_byte_order_mark(std::string_view bytes) noexcept
{
    if (bytes.starts_with(byte_order_mark()))
    {
        return bytes.substr(byte_order_mark().size());
    }
    return bytes;
}

// BOM → 正しい UTF-8 → CP932 として正しい、の順に見る。UTF-8 が常に勝つ（決定 3）。
[[nodiscard]] std::expected<TextEncoding, EncodingFailure>
detect_encoding(std::string_view bytes) noexcept;
} // namespace nenenib::core
