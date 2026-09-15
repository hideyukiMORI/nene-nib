#include "TextEncoding.hpp"

#include "Utf8.hpp"

#include <cstddef>

namespace nenenib::core
{
namespace
{
// CP932 のバイト列の形（決定 3）。割り当てのある文字かどうかは見ない。そこは変換の仕事である。
[[nodiscard]] constexpr bool single_byte(unsigned char byte) noexcept
{
    return byte <= 0x7FU || (byte >= 0xA1U && byte <= 0xDFU);
}

[[nodiscard]] constexpr bool lead_byte(unsigned char byte) noexcept
{
    return (byte >= 0x81U && byte <= 0x9FU) || (byte >= 0xE0U && byte <= 0xFCU);
}

[[nodiscard]] constexpr bool trail_byte(unsigned char byte) noexcept
{
    return (byte >= 0x40U && byte <= 0x7EU) || (byte >= 0x80U && byte <= 0xFCU);
}

[[nodiscard]] bool code_page_932(std::string_view bytes) noexcept
{
    std::size_t index = 0;
    while (index < bytes.size())
    {
        const auto byte = static_cast<unsigned char>(bytes[index]);
        if (single_byte(byte))
        {
            ++index;
            continue;
        }
        const bool pair = lead_byte(byte) && index + 1 < bytes.size() &&
                          trail_byte(static_cast<unsigned char>(bytes[index + 1]));
        if (!pair)
        {
            return false;
        }
        index += 2;
    }
    return true;
}
} // namespace

std::expected<TextEncoding, EncodingFailure> detect_encoding(std::string_view bytes) noexcept
{
    // BOM は明示的なラベルである。後ろが正しい UTF-8 でなければ、CP932 へは落ちずに断る（決定 3）。
    if (bytes.starts_with(byte_order_mark()))
    {
        if (validate_utf8(without_byte_order_mark(bytes)).has_value())
        {
            return TextEncoding::utf8_bom;
        }
        return std::unexpected(EncodingFailure::undecodable);
    }
    if (validate_utf8(bytes).has_value())
    {
        return TextEncoding::utf8;
    }
    if (code_page_932(bytes))
    {
        return TextEncoding::shift_jis;
    }
    return std::unexpected(EncodingFailure::undecodable);
}
} // namespace nenenib::core
