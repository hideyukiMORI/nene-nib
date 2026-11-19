#pragma once

#include "LinkCardContent.hpp"
#include "LinkCardFormats.hpp"
#include "LinkCardPaint.hpp"
#include "LinkCardPlace.hpp"
#include "LinkCardTarget.hpp"

#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>

namespace nenenib::ui::win32
{
// リンクの札の文字組み・配置・描画・当たり（施主決定 D106・#1869 の段 L2・採用案の Card）。札は 1
// つ しか出ず、出している間だけ描くので、文字組みは描くたびに作る（打鍵と frame
// の道に保持を足さない）。 寸法は core::LinkCard.hpp の DIP、置き場は core::link_card_rect の 1
// 本。

// 札の中身を formats で組み、place のリンクの下（入らなければ上）に置いた描く値。文字組みを作れ
// なければ nullopt。題・開き方・案内は札の内側の幅で折り返し、宛先は等幅で語の途中でも折る
// （ホストだけ太字）。ボタンは右寄せで、左が secondary・右が primary。
[[nodiscard]] std::optional<LinkCardPaint> link_card_paint(IDWriteFactory &dwrite,
                                                           const LinkCardFormats &formats,
                                                           const LinkCardContent &content,
                                                           const LinkCardPlace &place);

// 札を描く（地 panel・縁 panel_border・題・宛先の枠と文字・開き方・案内・2 つのボタン）。宛先の
// ホストの外は muted の色で描く（描く先の brush を 1 つ作る）。
void paint_link_card(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                     const LinkCardPaint &paint);

// 点 (x, y) の下の部品。
[[nodiscard]] LinkCardTarget link_card_target_of(const LinkCardPaint &paint, std::int32_t x,
                                                 std::int32_t y) noexcept;
} // namespace nenenib::ui::win32
