#pragma once

#include "DocumentFormatKind.hpp"

namespace nenenib::application
{
// 保存の形の候補を Ctrl+P の面の部品で開く（ADR 0010 の追補 #1049 の決定 2 の (a)・(c)）。
// ステータスバーの右端の文字コード・改行の項目のクリックと、F1 の操作「文字コードを選ぶ」「改行を
// 選ぶ」が送る。候補を選ぶと SelectDocumentFormat になる。使う人の操作の側。
struct OpenDocumentFormatList
{
    core::DocumentFormatKind kind;
};
} // namespace nenenib::application
