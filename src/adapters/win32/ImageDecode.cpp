// CLSID_WICImagingFactory2・GUID_ContainerFormat*・GUID_WICPixelFormat32bppPBGRA は
// wincodec.h の DEFINE_GUID で、定義は windowscodecs.lib にある。adapters は
// その lib を結ばない（exe が windowscodecs.dll を import しないので、起動の道で
// 読まない・Win32Printer と同じ形）ので、この翻訳単位で INITGUID の形で定義を
// 置く（selectany）。
#include <initguid.h>

#include <wincodec.h>

#include "ImageDecode.hpp"

#include "ImageGeometry.hpp"
#include "ImageLimits.hpp"
#include "ImageSize.hpp"

#include <objbase.h>
#include <wrl/client.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::ImageFailure;
using Pixels = std::shared_ptr<const application::ImagePixels>;
using Decoded = std::expected<Pixels, Failure>;
using Microsoft::WRL::ComPtr;
using Frame = std::expected<ComPtr<IWICBitmapFrameDecode>, Failure>;

[[nodiscard]] Failure failure_of(HRESULT result) noexcept
{
    if (result == E_OUTOFMEMORY)
    {
        return Failure::insufficient_memory;
    }
    if (result == WINCODEC_ERR_COMPONENTNOTFOUND)
    {
        return Failure::unsupported;
    }
    return Failure::corrupt;
}

// 先頭のバイト列で決めた形式の WIC の入れ物の GUID（知らない形式は無い）。
[[nodiscard]] const GUID *container_of(core::ImageFormat format) noexcept
{
    switch (format)
    {
    case core::ImageFormat::png:
        return &GUID_ContainerFormatPng;
    case core::ImageFormat::jpeg:
        return &GUID_ContainerFormatJpeg;
    case core::ImageFormat::gif:
        return &GUID_ContainerFormatGif;
    case core::ImageFormat::bmp:
        return &GUID_ContainerFormatBmp;
    case core::ImageFormat::unknown:
        return nullptr;
    }
    std::unreachable();
}

// 形式の復号器でバイト列を開き、最初の 1 コマを返す。バイト列は frame を使い終えるまで生かす。
[[nodiscard]] Frame first_frame(IWICImagingFactory &factory, std::span<char> bytes,
                                const GUID &container)
{
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    HRESULT result = factory.CreateStream(&stream);
    if (SUCCEEDED(result))
    {
        result = stream->InitializeFromMemory(std::bit_cast<BYTE *>(bytes.data()),
                                              static_cast<DWORD>(bytes.size()));
    }
    if (SUCCEEDED(result))
    {
        result = factory.CreateDecoder(container, nullptr, &decoder);
    }
    if (SUCCEEDED(result))
    {
        result = decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand);
    }
    if (SUCCEEDED(result))
    {
        result = decoder->GetFrame(0, &frame);
    }
    if (FAILED(result))
    {
        return std::unexpected(failure_of(result));
    }
    return frame;
}

// frame を 32bppPBGRA にし、size が元と違えば Fant で縮める。
[[nodiscard]] std::expected<ComPtr<IWICBitmapSource>, Failure>
scaled_source(IWICImagingFactory &factory, IWICBitmapFrameDecode &frame, core::ImageSize original,
              core::ImageSize size)
{
    ComPtr<IWICFormatConverter> converter;
    ComPtr<IWICBitmapSource> source;
    HRESULT result = factory.CreateFormatConverter(&converter);
    if (SUCCEEDED(result))
    {
        result =
            converter->Initialize(&frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                  nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
    }
    if (SUCCEEDED(result))
    {
        result = converter.As(&source);
    }
    ComPtr<IWICBitmapScaler> scaler;
    if (SUCCEEDED(result) && !(size == original))
    {
        result = factory.CreateBitmapScaler(&scaler);
        if (SUCCEEDED(result))
        {
            result = scaler->Initialize(converter.Get(), size.width, size.height,
                                        WICBitmapInterpolationModeFant);
        }
        if (SUCCEEDED(result))
        {
            result = scaler.As(&source);
        }
    }
    if (FAILED(result))
    {
        return std::unexpected(failure_of(result));
    }
    return source;
}

[[nodiscard]] Decoded pixels_of(IWICImagingFactory &factory, IWICBitmapFrameDecode &frame)
{
    UINT width = 0;
    UINT height = 0;
    const HRESULT sized = frame.GetSize(&width, &height);
    if (FAILED(sized))
    {
        return std::unexpected(failure_of(sized));
    }
    const core::ImageSize original{width, height};
    if (width == 0U || height == 0U)
    {
        return std::unexpected(Failure::corrupt);
    }
    if (!core::image_pixels_fit(original))
    {
        return std::unexpected(Failure::too_large);
    }
    const core::ImageSize size = core::image_decoded_size(original);
    const auto source = scaled_source(factory, frame, original, size);
    if (!source.has_value())
    {
        return std::unexpected(source.error());
    }
    // 画素は ImagePixels の大きさで 1 回だけ確保し、行の詰め物なしで写す（追補 53 の決まり）。
    const std::size_t row = std::size_t{size.width} * core::image_pixel_bytes;
    application::ImagePixels pixels{original, size,
                                    std::vector<std::uint8_t>(row * std::size_t{size.height})};
    const HRESULT copied = source.value()->CopyPixels(
        nullptr, static_cast<UINT>(row), static_cast<UINT>(pixels.bgra.size()), pixels.bgra.data());
    if (FAILED(copied))
    {
        return std::unexpected(failure_of(copied));
    }
    return std::make_shared<const application::ImagePixels>(std::move(pixels));
}

[[nodiscard]] Decoded decoded_in_apartment(std::span<char> bytes, core::ImageFormat format)
{
    const GUID *container = container_of(format);
    if (container == nullptr)
    {
        return std::unexpected(Failure::unsupported);
    }
    ComPtr<IWICImagingFactory> factory;
    const HRESULT made = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&factory));
    if (FAILED(made))
    {
        return std::unexpected(made == E_OUTOFMEMORY ? Failure::insufficient_memory
                                                     : Failure::unsupported);
    }
    const Frame frame = first_frame(*factory.Get(), bytes, *container);
    if (!frame.has_value())
    {
        return std::unexpected(frame.error());
    }
    return pixels_of(*factory.Get(), *frame.value().Get());
}
} // namespace

Decoded decoded_image(std::span<char> bytes, core::ImageFormat format) noexcept
{
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(apartment))
    {
        return std::unexpected(apartment == E_OUTOFMEMORY ? Failure::insufficient_memory
                                                          : Failure::unsupported);
    }
    // COM の物は decoded_in_apartment の中で放つので、CoUninitialize の前に全部消えている。
    Decoded decoded = std::unexpected(Failure::insufficient_memory);
    try
    {
        decoded = decoded_in_apartment(bytes, format);
    }
    catch (const std::bad_alloc &)
    {
        decoded = std::unexpected(Failure::insufficient_memory);
    }
    CoUninitialize();
    return decoded;
}
} // namespace nenenib::adapters::win32
