#pragma once

#include "PreviewInk.hpp"
#include "PreviewStyle.hpp"
#include "Theme.hpp"

#include <array>
#include <cstdint>
#include <d2d1.h>
#include <optional>
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
    // brush を作った回数（prepare の前は 0）。保持した layout に置いた色がどの brush を指すかを
    // PreviewTextLayouts が見分ける（作り直したら古い brush を指す layout を捨てる・#1477）。
    [[nodiscard]] std::uint64_t generation() const noexcept;

  private:
    static constexpr std::size_t ink_count = 11;
    ID2D1RenderTarget *target_ = nullptr;
    std::string theme_;
    std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, ink_count> brushes_{};
    std::uint64_t generation_ = 0;
};

// 行の中の装飾の文字の色（行内コードは string・リンクは function・画像の代わりの文字は comment・
// 強調と太字は周りの色のまま＝nullopt）。
[[nodiscard]] std::optional<PreviewInk> preview_span_ink(core::PreviewStyle style) noexcept;
} // namespace nenenib::ui::win32
