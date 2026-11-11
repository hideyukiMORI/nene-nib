#include "PreviewImageObject.hpp"

namespace nenenib::ui::win32
{
PreviewImageObject::PreviewImageObject(FLOAT width, FLOAT height) noexcept
    : width_(width), height_(height)
{
}

// SDK-ABI: IDWriteInlineObject::Draw
// NOLINTNEXTLINE(readability-function-size)
HRESULT STDMETHODCALLTYPE PreviewImageObject::Draw(void *, IDWriteTextRenderer *, FLOAT, FLOAT,
                                                   BOOL, BOOL, IUnknown *) noexcept
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
PreviewImageObject::GetMetrics(DWRITE_INLINE_OBJECT_METRICS *metrics) noexcept
{
    if (metrics == nullptr)
    {
        return E_POINTER;
    }
    *metrics = DWRITE_INLINE_OBJECT_METRICS{width_, height_, height_, FALSE};
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
PreviewImageObject::GetOverhangMetrics(DWRITE_OVERHANG_METRICS *overhangs) noexcept
{
    if (overhangs == nullptr)
    {
        return E_POINTER;
    }
    *overhangs = DWRITE_OVERHANG_METRICS{0.0F, 0.0F, 0.0F, 0.0F};
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PreviewImageObject::GetBreakConditions(
    DWRITE_BREAK_CONDITION *before, DWRITE_BREAK_CONDITION *after) noexcept
{
    if (before == nullptr || after == nullptr)
    {
        return E_POINTER;
    }
    *before = DWRITE_BREAK_CONDITION_CAN_BREAK;
    *after = DWRITE_BREAK_CONDITION_CAN_BREAK;
    return S_OK;
}
} // namespace nenenib::ui::win32
