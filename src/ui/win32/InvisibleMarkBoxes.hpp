#pragma once

#include "BodyGlyphRun.hpp"
#include "InvisibleBox.hpp"
#include "InvisibleMark.hpp"

#include <optional>
#include <span>
#include <vector>

namespace nenenib::ui::win32
{
// 不可視文字の印の升を、保持した字形 run の cluster と advance から引く（ADR 0193 の決定 10・
// #1977・構文の色の glyph_pieces と置き換えの replaced_tints と同じ写し・HitTestTextRange を回さ
// ない）。marks は前から順。返す升は layout の座標で、行の上端は uniform な行の高さ line_height の
// 倍数（BodyTextLayouts::set_spacing）。
// - 空白と全角空白は自分の cluster の字形の送りの区間。
// - Tab は DirectWrite が 0 字形の run にするので保持に無い。左端は前の字の右端（行頭は 0）、右端は
//   後ろの字が同じ画面行にあればその左端、無ければ左端の次の Tab の止まり（tab_stop の倍数・続く
//   Tab と行末の Tab）。
// - 保持に無い字（縦・横に画面外で集めなかった run
// の中）の空白と全角空白は返さない（見えていない）。
// 位置を引けない保持（右から左・横倒し・文字の対応が無い・文字の位置が前へ戻る）と、前の字の位置が
// 判らない Tab は nullopt で、呼び手は区間ごとの HitTestTextRange に戻る。
[[nodiscard]] std::optional<std::vector<InvisibleBox>>
invisible_boxes(std::span<const BodyGlyphRun> runs, std::span<const InvisibleMark> marks,
                float line_height, float tab_stop);
} // namespace nenenib::ui::win32
