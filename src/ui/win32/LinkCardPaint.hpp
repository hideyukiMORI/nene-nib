#pragma once

#include "LinkCardInks.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <utility>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 描くリンクの札 1 つ（施主決定 D106・#1869 の段 L2）。link_card_paint が作り、paint_link_card が
// 描き、link_card_target_of が当たりを引く（描く値と当たりが同じ矩形）。座標は描画先の物理画素。
// texts と origins は link_card_text の番号の順（題・宛先・開き方・案内・左のボタン・右のボタン）、
// emphasis は宛先の layout の中の太字の範囲（UTF-16 の [first, second)）と全体の長さ。公開
// aggregate（CPP-003）。
struct LinkCardPaint
{
    D2D1_RECT_F box;
    D2D1_RECT_F target_box;
    D2D1_RECT_F secondary;
    D2D1_RECT_F primary;
    std::array<Microsoft::WRL::ComPtr<IDWriteTextLayout>, 6> texts;
    std::array<D2D1_POINT_2F, 6> origins;
    std::pair<std::uint32_t, std::uint32_t> emphasis;
    std::uint32_t target_length;
    float radius;
    float target_radius;
    float button_radius;
    float stroke;
    LinkCardInks inks;
};

// texts と origins の番号。
namespace link_card_text
{
inline constexpr std::size_t title = 0;
inline constexpr std::size_t target = 1;
inline constexpr std::size_t way = 2;
inline constexpr std::size_t hint = 3;
inline constexpr std::size_t secondary = 4;
inline constexpr std::size_t primary = 5;
} // namespace link_card_text
} // namespace nenenib::ui::win32
