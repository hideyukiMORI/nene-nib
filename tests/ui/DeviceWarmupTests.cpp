// ADR 0131: worker が先に作る device の箱は、値を 1 回だけ渡し、stop 済みなら値を置かない。
// make() は試験のスレッドで呼ぶ（スレッドを作らない）。device を作れない機械では測らない。
#include "DeviceWarmup.hpp"
#include "WindowChecks.hpp"

namespace nenenib::tests::ui
{
namespace
{
void verify_stopped_make()
{
    ::nenenib::ui::win32::DeviceWarmup warmup;
    warmup.make(true);
    expect(!warmup.take().has_value(), "a stopped make leaves the warmup box empty");
}

void verify_single_take()
{
    ::nenenib::ui::win32::DeviceWarmup warmup;
    warmup.make(false);
    const auto first = warmup.take();
    if (!first.has_value())
    {
        unmeasured("D3D11 device for the warmup box (neither hardware nor WARP)");
        return;
    }
    const auto &warmed = first.value();
    expect(warmed.device && warmed.context && warmed.factory,
           "the first take hands over the device, its context and the DXGI factory");
    expect(!warmup.take().has_value(), "the second take is empty");
}
} // namespace

void verify_device_warmup()
{
    verify_stopped_make();
    verify_single_take();
}
} // namespace nenenib::tests::ui
