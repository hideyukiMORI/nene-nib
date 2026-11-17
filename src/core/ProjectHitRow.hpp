#pragma once

#include "Column.hpp"
#include "LineNumber.hpp"

#include <cstddef>
#include <string>

namespace nenenib::core
{
// 全文検索の当たりの面の 1 行の表示値（ADR 0167 の決定 5・ADR 0189 の当たりの行）。作るのは
// project_hit_row の 1 本。ui は 2 段（上に相対パスと行と桁・下に本文の窓と当たりの色）でも、今の
// 1 行の形（project_hit_choice の label と detail）でも、この値から描く。値だけを運ぶ公開
// aggregate（CPP-003）。
struct ProjectHitRow
{
    // ルートからの相対パス（UTF-8・区切りはファイルのパスのまま）。ルートの下でなければ全体。
    std::string path;
    // 当たりの行と桁（ProjectHit と同じ単位・1 始まり）。
    LineNumber line;
    Column column;
    // 行の本文の窓（ProjectHit::line_text の制御文字を 1 バイトの空白に置き換えた形。バイトの長さと
    // 位置は line_text と同じ）。
    std::string text;
    // text の中の当たりの半開区間（バイト・text の先頭から）。
    std::size_t match_begin;
    std::size_t match_end;
    // 窓が行頭から始まっていない（ProjectHit::text_begin が正）。ui は窓の前に省いた印を描く。
    bool elided;
};
} // namespace nenenib::core
