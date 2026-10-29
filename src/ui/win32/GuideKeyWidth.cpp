#include "GuideKeyWidth.hpp"

#include "Utf16.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
namespace
{
// 測るための layout の最大幅。書式は折り返さない（NO_WRAP）ので、鍵の 1 行の幅はこれに依らない。
constexpr float measuring_extent = 65536.0F;

// 字の幅（物理画素）。測れなければ負。
[[nodiscard]] float text_width(IDWriteFactory &factory, IDWriteTextFormat &format,
                               const std::string &key)
{
    const std::wstring wide = core::to_utf16(key).value_or(std::wstring{});
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(factory.CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()), &format,
                                        measuring_extent, measuring_extent, &layout)) ||
        FAILED(layout->GetMetrics(&metrics)))
    {
        return -1.0F;
    }
    return metrics.width;
}
} // namespace

std::int32_t guide_key_width(IDWriteFactory &factory, IDWriteTextFormat &format,
                             const std::array<core::GuideEntry, 3> &entries, float padding)
{
    float widest = 0.0F;
    for (const auto &entry : entries)
    {
        const float width = text_width(factory, format, entry.key);
        if (width < 0.0F)
        {
            return 0;
        }
        widest = std::max(widest, width);
    }
    return static_cast<std::int32_t>(std::ceil(widest + (padding * 2.0F)));
}
} // namespace nenenib::ui::win32
