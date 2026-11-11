#include "PreviewBitmaps.hpp"

#include "ImageLimits.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr std::size_t pixels_slot = 1;
constexpr std::size_t bitmap_slot = 2;
constexpr std::size_t bytes_slot = 3;
constexpr std::size_t used_slot = 4;
} // namespace

void PreviewBitmaps::begin(const ID2D1DeviceContext &context)
{
    ++paint_;
    if (context_ != &context)
    {
        release();
        context_ = &context;
        return;
    }
    const auto expired = std::ranges::remove_if(held_, [](const Entry &entry)
                                                { return std::get<0>(entry).expired(); });
    for (auto at = expired.begin(); at != expired.end(); ++at)
    {
        bytes_ -= std::get<bytes_slot>(*at);
    }
    held_.erase(expired.begin(), expired.end());
}

HRESULT PreviewBitmaps::hold(ID2D1DeviceContext &context,
                             const std::shared_ptr<const application::ImagePixels> &pixels)
{
    const auto found =
        std::ranges::find_if(held_, [&pixels](const Entry &entry)
                             { return std::get<pixels_slot>(entry) == pixels.get(); });
    if (found != held_.end())
    {
        std::get<used_slot>(*found) = paint_;
        return S_OK;
    }
    const auto size = D2D1::SizeU(pixels->size.width, pixels->size.height);
    // 画素は ImagePixels の決まり（32bppPBGRA・1 行 width * 4 バイト）なので、そのまま写す。
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    const auto made = context.CreateBitmap(
        size, pixels->bgra.data(), size.width * static_cast<UINT32>(core::image_pixel_bytes),
        properties, &bitmap);
    if (FAILED(made))
    {
        return made;
    }
    ++created_;
    bytes_ += pixels->bgra.size();
    held_.emplace_back(pixels, pixels.get(), std::move(bitmap), pixels->bgra.size(), paint_);
    trim();
    return S_OK;
}

void PreviewBitmaps::trim()
{
    while (bytes_ > core::preview_image_held_byte_limit)
    {
        const auto oldest = std::ranges::min_element(held_, {}, [](const Entry &entry)
                                                     { return std::get<used_slot>(entry); });
        if (oldest == held_.end() || std::get<used_slot>(*oldest) == paint_)
        {
            return;
        }
        bytes_ -= std::get<bytes_slot>(*oldest);
        held_.erase(oldest);
    }
}

ID2D1Bitmap1 *PreviewBitmaps::find(const application::ImagePixels *pixels) const
{
    const auto found = std::ranges::find_if(
        held_, [pixels](const Entry &entry)
        { return std::get<pixels_slot>(entry) == pixels && !std::get<0>(entry).expired(); });
    return found == held_.end() ? nullptr : std::get<bitmap_slot>(*found).Get();
}

void PreviewBitmaps::release() noexcept
{
    held_.clear();
    bytes_ = 0;
}

std::size_t PreviewBitmaps::count() const noexcept
{
    return held_.size();
}

std::size_t PreviewBitmaps::bytes() const noexcept
{
    return bytes_;
}

std::size_t PreviewBitmaps::created() const noexcept
{
    return created_;
}
} // namespace nenenib::ui::win32
