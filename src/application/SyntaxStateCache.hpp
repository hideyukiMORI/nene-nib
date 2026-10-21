#pragma once

#include "AutosaveId.hpp"
#include "Language.hpp"
#include "LexState.hpp"
#include "LineNumber.hpp"

namespace nenenib::application
{
// 枠ごとの構文の状態の覚え（#816・ADR 0156 の決定 9 と追補 #827）。文書（tab）を言語 language で
// 走らせたとき、行 line の行頭に入る字句の状態が state。controller が枠ごとに 1 つ持ち、
// (tab, language) が同じで line が見えている最初の行の手前 4,096 行以内なら、遡りの代わりにここ
// から続ける。改訂番号は鍵に入れない: 編集の 1 か所（replace・undo・redo）が、編集の始まりの行が
// line より前なら捨てる（line 以降の編集は line の行頭の状態を変えない）。EditorState には入れない
// （値型で写しが多い）。どのメンバーも単独で妥当な値なので公開 aggregate。
struct SyntaxStateCache
{
    AutosaveId tab;
    core::Language language;
    core::LineNumber line;
    core::LexState state;
};
} // namespace nenenib::application
