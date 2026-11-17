#pragma once

#include "LineNumber.hpp"
#include "PreviewColor.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nenenib::application
{
// プレビュー欄の窓の 1 行（ADR 0189 の決定 4・5）。作るのは preview_lines の 1 本。値だけを運ぶ
// 公開 aggregate（CPP-003）。
struct PreviewLine
{
    // 行番号（1 始まり・本文の行の番号）。
    core::LineNumber number;
    // 行の描画の形（改行を除く本文の最大 finder_preview_line_bytes バイトをコードポイントを割らずに
    // 切り、本文と同じ core::display_line で制御文字を `^X`・書式用文字を `<xxxx>` にした形）。
    std::string text;
    // 全文検索の当たりの行か（current_line の面を敷き、行番号を text の色で描く）。
    bool hit = false;
    // 当たりの文字の半開区間（バイト・text の先頭から・text の長さで切る）。等しければ面を
    // 敷かない。
    std::size_t match_begin = 0;
    std::size_t match_end = 0;
    // 構文の色の区間（text の中のバイト・窓の先頭の行から字句器を走らせた色）。
    std::vector<PreviewColor> colors = {};
    // 行の本文（描画の形にする前の切った本文）。当たりの印を今の当たりから付け直すときに、本文の
    // バイトの範囲を描画の形へ写すのに読む（marked_lines）。
    std::string source = {};
};
} // namespace nenenib::application
