#pragma once

#include "PreviewAlert.hpp"
#include "PreviewInk.hpp"
#include "PreviewStyle.hpp"
#include "SyntaxRole.hpp"
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
// 鍵は描画先とテーマの名前。描画先が変われば作り直し、同じ描画先でテーマだけが変われば同じ brush の
// 色を置き直す（SetColor・作った brush と同じ色と不透明度・#1485）。
class PreviewBrushes final
{
  public:
    [[nodiscard]] HRESULT prepare(ID2D1RenderTarget &target, const core::Theme &theme);
    // prepare の前は nullptr。
    [[nodiscard]] ID2D1SolidColorBrush *of(PreviewInk ink) const;
    // brush を作った回数（prepare の前は 0）。保持した layout に置いた色がどの brush を指すかを
    // PreviewTextLayouts が見分ける（作り直したら古い brush を指す layout を捨てる・#1477）。テーマ
    // だけの変更（色の置き直し）では変わらない（#1485）。
    [[nodiscard]] std::uint64_t generation() const noexcept;

  private:
    static constexpr std::size_t ink_count = 17;
    // 作った brush の色をテーマの色に置き直す（作り直さない）。
    void recolour(const core::Theme &theme);
    ID2D1RenderTarget *target_ = nullptr;
    std::string theme_;
    std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, ink_count> brushes_{};
    std::uint64_t generation_ = 0;
};

// 行の中の装飾の文字の色（行内コードは string・リンクは function・画像の代わりの文字は comment・
// 強調と太字は周りの色のまま＝nullopt）。
// alert の種類の名前は block の alert の色（preview_alert_ink）なのでここでは nullopt。
[[nodiscard]] std::optional<PreviewInk> preview_span_ink(core::PreviewStyle style) noexcept;
// alert の左の線と種類の名前の色（NOTE は function・TIP は string・IMPORTANT は number・WARNING は
// warning・CAUTION は error・採用案 D103・#1848）。
[[nodiscard]] PreviewInk preview_alert_ink(core::PreviewAlert alert) noexcept;
// 囲みのコードの構文の色の役割（#1858）。本文の syntax_color と同じ SyntaxPalette の欄を引く。
[[nodiscard]] PreviewInk preview_syntax_ink(core::SyntaxRole role) noexcept;
} // namespace nenenib::ui::win32
