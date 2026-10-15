#include "DeviceWarmup.hpp"

#include <dxgi1_3.h>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
// flags と feature levels は Direct2DRenderer::create_device と同じ（ADR 0131）。
HRESULT create_device(D3D_DRIVER_TYPE driver, WarmedDevice &warmed)
{
    constexpr UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    return D3D11CreateDevice(nullptr, driver, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
                             &warmed.device, nullptr, &warmed.context);
}

// event を待ち、作れていれば 1 回だけ渡す。take_provisional と take の共通の形。
// 結果は worker が書くので、参照で受けて event を待った後に読む。
std::optional<WarmedDevice> take_once(HANDLE event, const HRESULT &result, WarmedDevice &warmed,
                                      bool &taken)
{
    if (event == nullptr || taken)
    {
        return std::nullopt;
    }
    taken = true;
    if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0 || FAILED(result))
    {
        return std::nullopt;
    }
    return std::exchange(warmed, WarmedDevice{});
}
} // namespace

// 手動リセットの event。立てたら下ろさないので、何度待っても戻る。
DeviceWarmup::DeviceWarmup()
    : provisional_ready_(CreateEventW(nullptr, TRUE, FALSE, nullptr), &::CloseHandle),
      ready_(CreateEventW(nullptr, TRUE, FALSE, nullptr), &::CloseHandle)
{
}

// 段ごとに、渡す口（event）が無ければ作らない（渡せない device を作らない）。
void DeviceWarmup::make(bool stop_requested)
{
    if (provisional_ready_)
    {
        if (!stop_requested)
        {
            provisional_result_ = create_device(D3D_DRIVER_TYPE_WARP, provisional_);
        }
        static_cast<void>(SetEvent(provisional_ready_.get()));
    }
    if (!ready_)
    {
        return;
    }
    if (!stop_requested)
    {
        result_ = create_device(D3D_DRIVER_TYPE_HARDWARE, warmed_);
        if (FAILED(result_))
        {
            // GPU が無い環境では 2 つ目の WARP（SPECIFICATION 第 9 節・ADR 0132 の限界）。
            result_ = create_device(D3D_DRIVER_TYPE_WARP, warmed_);
        }
        if (SUCCEEDED(result_))
        {
            result_ = CreateDXGIFactory2(0, IID_PPV_ARGS(&warmed_.factory));
        }
    }
    static_cast<void>(SetEvent(ready_.get()));
}

std::optional<WarmedDevice> DeviceWarmup::take_provisional()
{
    return take_once(provisional_ready_.get(), provisional_result_, provisional_,
                     provisional_taken_);
}

std::optional<WarmedDevice> DeviceWarmup::take()
{
    return take_once(ready_.get(), result_, warmed_, taken_);
}

bool DeviceWarmup::ready() const
{
    return !ready_ || WaitForSingleObject(ready_.get(), 0) == WAIT_OBJECT_0;
}

HANDLE DeviceWarmup::event() const
{
    return ready_.get();
}
} // namespace nenenib::ui::win32
