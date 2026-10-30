#pragma once

#include "DocumentFormat.hpp"

namespace nenenib::application
{
// 現在の文書の保存の形（文字コードか改行の形）を選ぶ（ADR 0010 の追補 #1049 の決定 2）。入口は
// ステータスバーの面・`:set fenc=` `:set ff=`・F1 の操作の 3 つで、どれもこの 1 意図を送る。
// 変わるのは保存の形だけで、本文のバイト列は変えず undo にも載せない（決定
// 1）。使う人の操作なので、 歩きを終え 1 意図の知らせを消す側。
struct SelectDocumentFormat
{
    core::DocumentFormat format;
};
} // namespace nenenib::application
