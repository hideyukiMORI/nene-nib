#pragma once

#include "EditorFrame.hpp"
#include "RenderFailure.hpp"

#include <windows.h>

#include <d2d1_3.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <expected>
#include <memory>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// Phase 0 の D1 で実測した提示経路をそのまま持つ（ADR 0007）。
// D3D11 device → composition swap chain（flip・waitable）→ DirectComposition → D2D device context。
class Direct2DRenderer final
{
  public:
    [[nodiscard]] static std::expected<Direct2DRenderer, RenderFailure> create(HWND window,
                                                                               float dpi);
    [[nodiscard]] std::expected<void, RenderFailure> render(const application::EditorFrame &frame);
    [[nodiscard]] std::expected<void, RenderFailure> resize(UINT width, UINT height);
    void set_dpi(float dpi) noexcept;

  private:
    // 待機可能オブジェクトは HANDLE なので所有を型で閉じる（CPP-016）。
    using WaitableHandle = std::unique_ptr<void, decltype(&::CloseHandle)>;

    Direct2DRenderer() = default;
    [[nodiscard]] std::expected<void, RenderFailure> initialize(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> create_device();
    [[nodiscard]] std::expected<void, RenderFailure> create_swap_chain(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> bind_composition(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> create_context();
    [[nodiscard]] std::expected<void, RenderFailure> create_text_format();
    HRESULT try_face(const wchar_t *face);
    [[nodiscard]] std::expected<void, RenderFailure> draw(const application::EditorFrame &frame,
                                                          ID2D1Bitmap1 *surface);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device_;
    Microsoft::WRL::ComPtr<IDXGISwapChain2> swap_chain_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> composition_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> target_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> visual_;
    Microsoft::WRL::ComPtr<ID2D1Factory7> factory_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext6> context_;
    Microsoft::WRL::ComPtr<IDWriteFactory7> dwrite_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
    WaitableHandle latency_{nullptr, &::CloseHandle};
    float dpi_ = 96.0F;
};
} // namespace nenenib::ui::win32
