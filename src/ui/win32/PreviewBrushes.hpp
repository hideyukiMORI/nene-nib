#pragma once

#include "PreviewInk.hpp"
#include "Theme.hpp"

#include <array>
#include <d2d1.h>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// プレビューの役割ごとの brush（ADR 0151 の決定 17）。行の中の装飾（リンク・行内コード）は
// layout の範囲に drawing effect として brush を置くので、役割ごとに別の brush を持つ。
// 鍵は描画先とテーマの名前で、どちらかが変われば作り直す。
class PreviewBrushes final
{
  public:
    [[nodiscard]] HRESULT prepare(ID2D1RenderTarget &target, const core::Theme &theme);
    // prepare の前は nullptr。
    [[nodiscard]] ID2D1SolidColorBrush *of(PreviewInk ink) const;

  private:
    static constexpr std::size_t ink_count = 8;
    ID2D1RenderTarget *target_ = nullptr;
    std::string theme_;
    std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, ink_count> brushes_{};
};
} // namespace nenenib::ui::win32
