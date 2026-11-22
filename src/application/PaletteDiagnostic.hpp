#pragma once

#include "PreviewAnchor.hpp"
#include "TextPosition.hpp"

#include <cstddef>

namespace nenenib::application
{
// Ctrl+P の診断の候補 1 つが指す所（#1939）。面を開くときに controller が控え、候補の command は
// この列の位置。tab は面を開いたときの帯の位置。開いている間に ui は帯のクリックを受けないが、
// 裏で読めなかったタブが帯から外れると（reach_tab の dropped）位置はずれ得るので、読む所は範囲を
// 確かめる（プレビューは帯の外なら無し・選ぶ道は SwitchTab の範囲の検査）。at はその文書の本文の
// 行と桁（古ければ placed_caret が丸める）、anchor はプレビュー欄の窓の中心と印（ADR 0189）。
// 公開 aggregate（CPP-003）。
struct PaletteDiagnostic
{
    std::size_t tab;
    core::TextPosition at;
    PreviewAnchor anchor;
};
} // namespace nenenib::application
