#pragma once

#include "LanguageQueryKind.hpp"

namespace nenenib::application
{
// 言語サーバーへキャレットの位置で問う（ADR 0190 の決定 8）。操作「定義へ移動」（F12）と「情報を
// 表示」が送る。Vim の `gd` `K` は engine の効果（core::VimLanguageQuery）から同じ 1 本
// （EditorController::query_language）へ入る。言語サーバーが動いていない文書では 1 行知らせる。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct QueryLanguage
{
    core::LanguageQueryKind kind;
};
} // namespace nenenib::application
