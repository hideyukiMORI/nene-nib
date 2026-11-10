#include "ImageGeometry.hpp"

#include "ImageLimits.hpp"

#include <cstdint>

namespace nenenib::core
{
namespace
{
// side × numerator ÷ denominator を四捨五入し、1 より小さくしない。辺は 32 bit なので 64 bit の積は
// 溢れない。denominator は 0 でない。
[[nodiscard]] std::uint32_t scaled_side(std::uint32_t side, std::uint32_t numerator,
                                        std::uint32_t denominator) noexcept
{
    const std::uint64_t product = std::uint64_t{side} * numerator;
    const std::uint64_t rounded = (product + (denominator / 2U)) / denominator;
    return rounded == 0U ? 1U : static_cast<std::uint32_t>(rounded);
}
} // namespace

bool image_pixels_fit(ImageSize original) noexcept
{
    return original.width != 0U && original.height != 0U &&
           std::uint64_t{original.width} * original.height <= image_pixel_limit;
}

ImageSize image_decoded_size(ImageSize original) noexcept
{
    if (original.width <= image_decoded_side_limit && original.height <= image_decoded_side_limit)
    {
        return original;
    }
    if (original.width >= original.height)
    {
        return ImageSize{image_decoded_side_limit,
                         scaled_side(original.height, image_decoded_side_limit, original.width)};
    }
    return ImageSize{scaled_side(original.width, image_decoded_side_limit, original.height),
                     image_decoded_side_limit};
}

ImageSize image_placement(ImageSize original, std::uint32_t frame_width) noexcept
{
    if (original.width == 0U || original.height == 0U || frame_width == 0U)
    {
        return ImageSize{0U, 0U};
    }
    const ImageSize decoded = image_decoded_size(original);
    if (decoded.width <= frame_width)
    {
        return decoded;
    }
    return ImageSize{frame_width, scaled_side(original.height, frame_width, original.width)};
}
} // namespace nenenib::core
