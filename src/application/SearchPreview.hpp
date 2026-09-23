#pragma once

#include "ScrollState.hpp"
#include "TextPosition.hpp"

#include <optional>

namespace nenenib::application
{
// incsearch の入力中だけある当たりの見せ方（ADR 0041 の決定 2）。engine のキャレットと
// last_search は確定まで動かないので、見せる位置はこの別欄が持つ。match が無いのは「入力中だが
// 当たりを見せない」（incsearch off・入力が空・解析の失敗・不一致）で、そのとき画面は origin
// （入力行を開いたときのスクロール）に戻る。2 つの欄は互いに独立して妥当なので公開 aggregate。
struct SearchPreview
{
    std::optional<core::TextPosition> match;
    ScrollState origin;
};
} // namespace nenenib::application
