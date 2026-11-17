#pragma once

#include "DisplayText.hpp"
#include "PreviewLine.hpp"

#include <optional>
#include <string>
#include <vector>

namespace nenenib::application
{
// プレビュー欄の表示値（ADR 0189 の決定 4・5）。見出しは選んだ候補のもので、本文の窓は届いた分だけ
// （選択が動いた瞬間に前の中身を残さない）。段 P1 は開いているタブの文書だけ手元の本文から窓を
// 作り、ほかの候補は見出しと notice の 1 行だけ。値だけを運ぶ公開 aggregate（CPP-003）。
struct PreviewSnippet
{
    // 見出しの名前（ファイルの名前・無題はタブの題名）。text の色の semibold で描く。
    core::DisplayText name;
    // 見出しの場所（ファイルのあるフォルダ・無題は無し）。
    std::optional<core::DisplayText> place;
    // 見出しの「412 行目」（全文検索の当たりだけ・表示言語の文）。
    std::optional<core::DisplayText> line;
    // 見出しの言語の名前（Vim の filetype 名・plain は text）。
    std::string language;
    // 本文の窓（最大 finder_preview_line_limit 行・上から順）。
    std::vector<PreviewLine> lines;
    // 中身を出せないときの 1 行（段 P1 では開いていないファイル）。値があれば lines は空。
    std::optional<core::DisplayText> notice;
};
} // namespace nenenib::application
