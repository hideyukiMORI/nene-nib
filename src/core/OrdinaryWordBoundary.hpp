#pragma once

#include "Offset.hpp"
#include "OrdinaryWordDirection.hpp"

#include <cstddef>

namespace nenenib::core
{
class TextBuffer;

// 通常モードの語の境の正本（ADR 0009 の追補 #1394）。Ctrl+← → の着地と、Ctrl+Backspace /
// Ctrl+Delete が消す範囲の端はこの 1 本で決める。at は UTF-8 の code point 境界。
//
// 語は同じ種類の文字の連なりで、後ろに続く空白（' '・U+00A0・U+3000 など Vim の表の空白）は
// 前の語に付く。種類は Vim の語の分類の表（vim_character_class の word）を引き、Windows の
// メモ帳（Office RichEdit）の実測に合わせて 4 か所だけ読み替える: '_' は記号、Tab は 1 つずつ
// 別の語（空白に数えない）、半角カタカナ（U+FF66〜U+FF9F）は英数字と別の種類、英数字に挟まれた
// '\'' と U+2019 は語の中。幅 0 の文字（結合文字）は前の文字と同じ語に付く。
//
// forward（Ctrl+→）: 行末なら改行 1 つを越えて次の行頭。空白の上なら空白を越える。Tab の上なら
// その Tab と後ろの空白を越える。それ以外は同じ種類の連なりと後ろの空白を越える。
// backward（Ctrl+←）: 行頭なら改行 1 つを越えて前の行末。前の空白を越え、そこが行頭なら（字下げ）
// 改行を越えて前の行末へ着き、空の行が続けば全部越える。そうでなければ Tab 1 つか、同じ種類の
// 連なりの頭へ戻る。本文の頭と末尾では動かない。
//
// 1 打鍵ごとに呼ばれるので行の全体は写さない。キャレットの前後の窓（4 KiB）を読み、判断が窓の端に
// 掛かれば窓を倍にして読み直す（#1394 のレビュー M1）。
[[nodiscard]] Offset ordinary_word_boundary(const TextBuffer &text, Offset at,
                                            OrdinaryWordDirection direction);
// 最初の窓の大きさを決める口（試験が 1 バイトの窓から確かめる）。答えは窓の大きさに依らない。
[[nodiscard]] Offset ordinary_word_boundary(const TextBuffer &text, Offset at,
                                            OrdinaryWordDirection direction,
                                            std::size_t window_bytes);
} // namespace nenenib::core
