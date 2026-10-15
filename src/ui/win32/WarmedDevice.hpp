#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// worker が先に作った D3D11 の device と DXGI の factory。
// DeviceWarmup が 1 回だけ渡す（ADR 0131）。
struct WarmedDevice final
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
};
} // namespace nenenib::ui::win32
