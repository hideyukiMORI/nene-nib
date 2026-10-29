#pragma once

#include <cstddef>

namespace nenenib::application
{
// プレビューのタスクリストのチェックボックスを切り替える（ADR 0155 の追補 24）。使う人の操作で、
// block は見せている結果（PreviewView.document）の要素の番号。controller は要素の印の 1 バイトを
// ` ` ↔ `x` に置き換える Edit 1 つにする。
struct TogglePreviewTask
{
    std::size_t block;
};
} // namespace nenenib::application
