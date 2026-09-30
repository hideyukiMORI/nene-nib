#pragma once

#include "DocumentView.hpp"
#include "FilePath.hpp"
#include "LineNumber.hpp"
#include "TextPosition.hpp"

namespace nenenib::application
{
// まだ読んでいない文書（ADR 0059 の決定 4）。前回のタブの一覧から帯に並べたタブで、初めて見るときに
// controller が読んで束 DocumentState に置き換える。状態はファイルに触れないので、この形のタブへは
// 切り替えられない。カーソルと画面の先頭の行は覚えていた値のまま（読んだ本文の範囲へ寄せるのは
// 読むとき）。view は題名 tab_title_for(path, saved) と、未保存ではない印（窓を閉じるときの確認の
// 対象にならない）。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct UnloadedDocument
{
    core::FilePath path;
    core::TextPosition caret;
    core::LineNumber first_visible;
    DocumentView view;
};
} // namespace nenenib::application
