#pragma once
#include "BodyGlyphRun.hpp"
#include "LayoutRect.hpp"

#include <d2d1.h>
#include <string_view>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace nenenib::tests::ui
{
// メモリの bitmap に Direct2D のソフトウェアの描画先で描き、画素を読み返す（ADR 0077 の決定 2）。
// 窓・device・swap chain・GPU を作らない。DPI は core::reference_dpi、変換は恒等。
class SoftwareTarget final
{
  public:
    using Pixels = std::vector<BYTE>;
    // 作れなければ ready() が false。
    SoftwareTarget();
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] float dpi() const;
    // DrawTextLayout（D2D1_DRAW_TEXT_OPTIONS_CLIP）の経路。失敗したら空。
    [[nodiscard]] Pixels text_layout(IDWriteTextLayout *layout, D2D1_POINT_2F origin);
    // DrawTextW と DrawTextLayout の既定 options の比較（行番号・題名、ADR 0083）。
    [[nodiscard]] Pixels text_w(std::wstring_view text, IDWriteTextFormat *format,
                                const core::LayoutRect &area);
    [[nodiscard]] Pixels plain_layout(IDWriteTextLayout *layout, const core::LayoutRect &area);
    // Direct2DRenderer::draw_body_text と同じく、保持した字形を領域の clip の中で描く経路。
    [[nodiscard]] Pixels glyph_runs(const std::vector<nenenib::ui::win32::BodyGlyphRun> &runs,
                                    D2D1_POINT_2F origin, D2D1_RECT_F clip);

  private:
    [[nodiscard]] Pixels finish();
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap_;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};
} // namespace nenenib::tests::ui
