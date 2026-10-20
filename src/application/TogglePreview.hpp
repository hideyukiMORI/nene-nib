#pragma once

namespace nenenib::application
{
// Markdown のプレビューを開く・閉じる（ADR 0155 の決定 3）。使う人の操作で、もう 1 つの枠が
// 無ければ左右に分けてプレビューを置き、本文の枠なら置き換え、プレビューなら閉じる。
struct TogglePreview
{
};
} // namespace nenenib::application
