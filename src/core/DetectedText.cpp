#include "DetectedText.hpp"

#include <utility>

namespace nenenib::core
{
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
    return DetectedText(std::move(owned), encoding.value());
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
