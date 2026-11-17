#pragma once

#include "LineNumber.hpp"

#include <cstddef>
#include <string>

namespace nenenib::application
{
// プレビュー欄の窓の 1 行（ADR 0189 の決定 4・5）。作るのは preview_lines の 1 本。値だけを運ぶ
// 公開 aggregate（CPP-003）。
struct PreviewLine
{
    // 行番号（1 始まり・本文の行の番号）。
    core::LineNumber number;
    // 行の本文（改行を除く・最大 finder_preview_line_bytes バイトでコードポイントを割らずに切る・
    // Tab のほかの制御文字は 1 バイトの空白に置き換えた形）。
    std::string text;
    // 全文検索の当たりの行か（current_line の面を敷き、行番号を text の色で描く）。
    bool hit = false;
    // 当たりの文字の半開区間（バイト・text の先頭から・text の長さで切る）。等しければ面を
    // 敷かない。
    std::size_t match_begin = 0;
    std::size_t match_end = 0;
};
} // namespace nenenib::application
