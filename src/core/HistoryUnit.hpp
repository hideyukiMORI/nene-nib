#pragma once

#include "Edit.hpp"
#include "VimChange.hpp"

namespace nenenib::core
{
// 履歴の 1 件（ADR 0181 の決定 3・4）。edit が空（removed も inserted も空）の件は本文に触れない
// 「空の単位」で、戻り先（restore）だけが意味を持つ。change は本文を変えたと数える単位か（Vim の
// UH_CHANGED と同じ答え）で、saved_only の件は保存時点からの距離（content_position）を進めない。
// 通常モードの編集はどれも changed。EditHistory だけが作り、外へは edit だけを返す。
struct HistoryUnit
{
    Edit edit;
    VimChange change;
};
} // namespace nenenib::core
