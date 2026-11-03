#pragma once

#include <d2d1_1.h>
#include <wincodec.h>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// print control を作る Direct2D の device と WIC の factory（ADR 0177 の決定 3）。製品は WARP の
// device を Win32Printer が作り、tests/ui はメモリの WIC bitmap の描画先の device を渡す。
struct PrintSurface
{
    Microsoft::WRL::ComPtr<ID2D1Device> device;
    Microsoft::WRL::ComPtr<IWICImagingFactory2> imaging;
};
} // namespace nenenib::ui::win32
