// ADR 0131 / 0132: worker が先に作る device の箱は、WARP（A）と本物（B）を 2 段で、それぞれ
// 1 回だけ渡し、stop 済みならどちらも値を置かない。
// make() は試験のスレッドで呼ぶ（スレッドを作らない）。device を作れない機械では測らない。
#include "DeviceWarmup.hpp"
#include "WindowChecks.hpp"

#include <dxgi.h>
#include <wrl/client.h>

namespace nenenib::tests::ui
{
namespace
{
// WARP の adapter は Microsoft Basic Render Driver（VendorId 0x1414・DeviceId 0x8C）。
bool is_warp(ID3D11Device *device)
{
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC description{};
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi))) || FAILED(dxgi->GetAdapter(&adapter)) ||
        FAILED(adapter->GetDesc(&description)))
    {
        return false;
    }
    return description.VendorId == 0x1414U && description.DeviceId == 0x8CU;
}

void verify_stopped_make()
{
    ::nenenib::ui::win32::DeviceWarmup warmup;
    expect(!warmup.ready(), "before make the real device is not ready");
    warmup.make(true);
    expect(warmup.ready(), "a stopped make still raises the real device's event");
    expect(!warmup.take_provisional().has_value(),
           "a stopped make leaves no provisional device in the box");
    expect(!warmup.take().has_value(), "a stopped make leaves the warmup box empty");
}

void verify_provisional_take()
{
    ::nenenib::ui::win32::DeviceWarmup warmup;
    warmup.make(false);
    expect(warmup.event() != nullptr && warmup.ready(),
           "after make the real device's event is raised and handed to the message loop");
    const auto first = warmup.take_provisional();
    if (!first.has_value())
    {
        unmeasured("WARP device for the warmup box");
        return;
    }
    const auto &warmed = first.value();
    expect(warmed.device && warmed.context, "the provisional take hands over a device and context");
    expect(is_warp(warmed.device.Get()), "the provisional device is WARP");
    expect(!warmup.take_provisional().has_value(), "the second provisional take is empty");
}

void verify_single_take()
{
    ::nenenib::ui::win32::DeviceWarmup warmup;
    warmup.make(false);
    const auto provisional = warmup.take_provisional();
    const auto first = warmup.take();
    if (!first.has_value())
    {
        unmeasured("D3D11 device for the warmup box (neither hardware nor WARP)");
        return;
    }
    const auto &warmed = first.value();
    expect(warmed.device && warmed.context && warmed.factory,
           "the first take hands over the device, its context and the DXGI factory");
    expect(!provisional.has_value() || provisional.value().device != warmed.device,
           "the real device is not the provisional WARP device");
    expect(!warmup.take().has_value(), "the second take is empty");
}
} // namespace

void verify_device_warmup()
{
    verify_stopped_make();
    verify_provisional_take();
    verify_single_take();
}
} // namespace nenenib::tests::ui
