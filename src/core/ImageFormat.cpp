#include "ImageFormat.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr std::array<std::uint8_t, 8> png_signature{0x89U, 0x50U, 0x4EU, 0x47U,
                                                    0x0DU, 0x0AU, 0x1AU, 0x0AU};
constexpr std::array<std::uint8_t, 3> jpeg_signature{0xFFU, 0xD8U, 0xFFU};
constexpr std::string_view gif87 = "GIF87a";
constexpr std::string_view gif89 = "GIF89a";
constexpr std::string_view bmp_signature = "BM";
// BMP の情報の頭（BITMAPCOREHEADER〜BITMAPV5HEADER）の大きさと、その 4 バイトの位置。
constexpr std::array<std::uint32_t, 7> bmp_header_sizes{12U, 40U, 52U, 56U, 64U, 108U, 124U};
constexpr std::size_t bmp_header_size_at = 14;
constexpr std::size_t bmp_header_size_bytes = 4;

[[nodiscard]] std::uint8_t byte_at(std::string_view head, std::size_t index) noexcept
{
    return static_cast<std::uint8_t>(head[index]);
}

template <std::size_t Size>
[[nodiscard]] bool starts_with_bytes(std::string_view head,
                                     const std::array<std::uint8_t, Size> &signature) noexcept
{
    if (head.size() < Size)
    {
        return false;
    }
    for (std::size_t index = 0; index < Size; ++index)
    {
        if (byte_at(head, index) != signature.at(index))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool starts_with_text(std::string_view head, std::string_view prefix) noexcept
{
    return head.size() >= prefix.size() && head.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool bitmap(std::string_view head) noexcept
{
    if (head.size() < bmp_header_size_at + bmp_header_size_bytes ||
        !starts_with_text(head, bmp_signature))
    {
        return false;
    }
    std::uint32_t size = 0;
    for (std::size_t index = bmp_header_size_bytes; index > 0; --index)
    {
        size = (size << 8U) | byte_at(head, bmp_header_size_at + index - 1U);
    }
    for (const std::uint32_t known : bmp_header_sizes)
    {
        if (size == known)
        {
            return true;
        }
    }
    return false;
}
} // namespace

ImageFormat image_format_of(std::string_view head) noexcept
{
    if (starts_with_bytes(head, png_signature))
    {
        return ImageFormat::png;
    }
    if (starts_with_bytes(head, jpeg_signature))
    {
        return ImageFormat::jpeg;
    }
    if (starts_with_text(head, gif87) || starts_with_text(head, gif89))
    {
        return ImageFormat::gif;
    }
    if (bitmap(head))
    {
        return ImageFormat::bmp;
    }
    return ImageFormat::unknown;
}
} // namespace nenenib::core
