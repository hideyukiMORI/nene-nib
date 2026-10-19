#include "DetectedText.hpp"

#include "Utf16.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// UTF-16 の本文を UTF-8 へ写す（ADR 0149 の決定 2）。ほかの文字コードは写さない（nullopt）。
// 判定が対と偶数を確かめてあるので失敗は起きないが、起きても開けない列として返す。
[[nodiscard]] std::optional<std::expected<std::string, EncodingFailure>>
utf16_as_utf8(std::string_view bytes, TextEncoding encoding)
{
    const auto converted = [bytes](std::size_t mark, ByteOrder order)
    {
        return utf8_of_utf16_bytes(bytes.substr(mark), order)
            .transform_error([](TextFailure) { return EncodingFailure::undecodable; });
    };
    switch (encoding)
    {
    case TextEncoding::utf8:
    case TextEncoding::utf8_bom:
    case TextEncoding::shift_jis:
        return std::nullopt;
    case TextEncoding::utf16_le:
        return converted(utf16_le_byte_order_mark().size(), ByteOrder::little);
    case TextEncoding::utf16_be:
        return converted(utf16_be_byte_order_mark().size(), ByteOrder::big);
    }
    std::unreachable();
}
} // namespace

DetectedText::DetectedText(std::string bytes, TextEncoding encoding)
    : bytes_(std::move(bytes)), encoding_(encoding)
{
}

std::expected<DetectedText, EncodingFailure> DetectedText::from_bytes(std::string_view bytes)
{
    std::string owned(bytes);
    const auto encoding = detect_encoding(owned);
    if (!encoding)
    {
        return std::unexpected(encoding.error());
    }
    return from_detected(std::move(owned), encoding.value());
}

std::expected<DetectedText, EncodingFailure> DetectedText::from_detected(std::string bytes,
                                                                         TextEncoding encoding)
{
    auto utf8 = utf16_as_utf8(bytes, encoding);
    if (!utf8.has_value())
    {
        return DetectedText(std::move(bytes), encoding);
    }
    if (!utf8.value())
    {
        return std::unexpected(utf8.value().error());
    }
    return DetectedText(std::move(utf8).value().value(), encoding);
}

std::expected<DetectedText, EncodingFailure> DetectedText::from_sealed(SealedBytes &&bytes)
{
    const auto encoding = detect_encoding(bytes.view());
    if (!encoding)
    {
        return std::unexpected(encoding.error());
    }
    return from_detected(std::move(bytes).release(), encoding.value());
}

TextEncoding DetectedText::encoding() const noexcept
{
    return encoding_;
}

std::string_view DetectedText::bytes() const & noexcept
{
    return bytes_;
}
} // namespace nenenib::core
