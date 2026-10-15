#include "DeviceWarmup.hpp"

#include <dxgi1_3.h>
#include <utility>

namespace nenenib::ui::win32
{
// 手動リセットの event。立てたら下ろさないので、take() が何度待っても戻る。
DeviceWarmup::DeviceWarmup() : ready_(CreateEventW(nullptr, TRUE, FALSE, nullptr), &::CloseHandle)
{
}

// flags・feature levels・WARP への切り替えは Direct2DRenderer::create_device と同じ（ADR 0131）。
void DeviceWarmup::make(bool stop_requested)
{
    if (!ready_)
    {
        return;
    }
    if (!stop_requested)
    {
        constexpr UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        result_ = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                    D3D11_SDK_VERSION, &warmed_.device, nullptr, &warmed_.context);
        if (FAILED(result_))
        {
            result_ =
                D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0,
                                  D3D11_SDK_VERSION, &warmed_.device, nullptr, &warmed_.context);
        }
        if (SUCCEEDED(result_))
        {
            result_ = CreateDXGIFactory2(0, IID_PPV_ARGS(&warmed_.factory));
        }
    }
    static_cast<void>(SetEvent(ready_.get()));
}

std::optional<WarmedDevice> DeviceWarmup::take()
{
    if (!ready_ || taken_)
    {
        return std::nullopt;
    }
    taken_ = true;
    if (WaitForSingleObject(ready_.get(), INFINITE) != WAIT_OBJECT_0 || FAILED(result_))
    {
        return std::nullopt;
    }
    return std::exchange(warmed_, WarmedDevice{});
}
} // namespace nenenib::ui::win32
