#pragma once

#include "VisualPosition.hpp"

namespace nenenib::application
{
// 画面行を数えて送った先（VisualTextView::reached）。stopped は本文の最初か最後の画面行で止まり、
// 頼まれた数を送りきれなかったこと（通常モードの上下の移動が本文の端へ着く合図・#1386）。
struct ShiftedPosition
{
    core::VisualPosition at;
    bool stopped;
};
} // namespace nenenib::application
