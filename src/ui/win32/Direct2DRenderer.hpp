#pragma once

#include "EditorFrame.hpp"
#include "LayoutRect.hpp"
#include "RenderFailure.hpp"
#include "RgbColor.hpp"
#include "RgbaColor.hpp"
#include "StatusBarLayout.hpp"
#include "TitleBarBackdrop.hpp"
#include "TitleBarLayout.hpp"

#include <windows.h>

#include <cstdint>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <expected>
#include <memory>
#include <string_view>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// Phase 0 の D1 で実測した提示経路をそのまま持つ（ADR 0007）。
// D3D11 device → composition swap chain（flip・waitable）→ DirectComposition → D2D device context。
// 座標は物理画素（context の DPI は 96 に固定し、寸法は core のレイアウト純関数が掛ける）。
class Direct2DRenderer final
{
  public:
    [[nodiscard]] static std::expected<Direct2DRenderer, RenderFailure> create(HWND window,
                                                                               std::uint32_t dpi);
    [[nodiscard]] std::expected<void, RenderFailure> render(const application::EditorFrame &frame);
    [[nodiscard]] std::expected<void, RenderFailure> resize(UINT width, UINT height);
    [[nodiscard]] std::expected<void, RenderFailure> set_dpi(std::uint32_t dpi);
    void set_backdrop(TitleBarBackdrop backdrop) noexcept;

  private:
    // 待機可能オブジェクトは HANDLE なので所有を型で閉じる（CPP-016）。
    using WaitableHandle = std::unique_ptr<void, decltype(&::CloseHandle)>;
    using TextFormat = Microsoft::WRL::ComPtr<IDWriteTextFormat>;

    Direct2DRenderer() = default;
    [[nodiscard]] std::expected<void, RenderFailure> initialize(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> create_device();
    [[nodiscard]] std::expected<void, RenderFailure> create_swap_chain(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> bind_composition(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> create_context();
    [[nodiscard]] std::expected<void, RenderFailure> create_text_formats();
    void align_text_formats();
    [[nodiscard]] const wchar_t *family(const wchar_t *preferred, const wchar_t *fallback) const;
    [[nodiscard]] HRESULT make_format(const wchar_t *face, float size_dips,
                                      DWRITE_FONT_WEIGHT weight, TextFormat &format);
    [[nodiscard]] float scaled(float dips) const noexcept;
    void fill(const core::LayoutRect &area, core::RgbColor color);
    void fill_translucent(const core::LayoutRect &area, core::RgbaColor color);
    void fill_rounded(const core::LayoutRect &area, core::RgbColor color, float radius);
    void write(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area,
               core::RgbColor color);
    void draw_cross(const core::LayoutRect &box, float half, float stroke);
    void draw_title_bar(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_tab(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_caption_glyphs(const core::TitleBarLayout &layout, core::RgbColor color);
    void draw_body(const application::EditorFrame &frame, const core::LayoutRect &area);
    void draw_status_bar(const application::EditorFrame &frame,
                         const core::StatusBarLayout &layout);
    void draw_toggle(const application::EditorFrame &frame, const core::StatusBarLayout &layout);
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
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<IDWriteFactory7> dwrite_;
    TextFormat tab_format_;
    TextFormat toggle_format_;
    TextFormat status_format_;
    TextFormat mode_format_;
    TextFormat gutter_format_;
    TextFormat code_format_;
    WaitableHandle latency_{nullptr, &::CloseHandle};
    std::uint32_t dpi_ = 96;
    TitleBarBackdrop backdrop_ = TitleBarBackdrop::opaque;
};
} // namespace nenenib::ui::win32
