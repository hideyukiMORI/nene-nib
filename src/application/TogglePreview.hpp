#pragma once

namespace nenenib::application
{
// Markdown のプレビューの見せ方を巡らせる（ADR 0155 の決定 3・追補 #1354・施主決定 D77）。使う人の
// 操作で、off → side → full → off（cycled_preview_placement）。side を開くときは、もう 1 つの
// 枠が無ければ左右に分けてプレビューを置き、本文の枠なら置き換える。
struct TogglePreview
{
};
} // namespace nenenib::application
