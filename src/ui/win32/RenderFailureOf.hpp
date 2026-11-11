#pragma once

#include "RenderFailure.hpp"

#include <d2d1.h>
#include <windows.h>

namespace nenenib::ui::win32
{
// 描画の HRESULT の失敗の種類（device lost か、それ以外の Direct2D の失敗か）。renderer の
// EndDraw・Present・surface の取得と、ミニマップの Flush（ADR 0171 の追補 #1449）が共用する 1 本。
[[nodiscard]] inline RenderFailure render_failure_of(HRESULT result) noexcept
{
    if (result == D2DERR_RECREATE_TARGET || result == DXGI_ERROR_DEVICE_REMOVED ||
        result == DXGI_ERROR_DEVICE_RESET)
    {
        return RenderFailure::device_lost;
    }
    return RenderFailure::direct2d;
}
} // namespace nenenib::ui::win32
