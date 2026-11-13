#pragma once

namespace nenenib::application
{
// 表示言語の候補を Ctrl+P の面で開く（ADR 0123 の追補 2026-11-13 の決定 8・#1019）。F1 の操作
// 「表示言語を選ぶ」が送る。面は `:set language=` の候補（4 つ）で開き、選ぶと Ex の
// `:set language=<名前>` と同じ 1 本で置く。使う人の操作の側。
struct OpenDisplayLanguageList
{
};
} // namespace nenenib::application
