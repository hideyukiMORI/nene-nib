#pragma once

#include "DisplayText.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"

#include <memory>
#include <optional>

namespace nenenib::application
{
// プレビューの枠の表示値（ADR 0155 の決定 11）。PreviewView の値の写しだけで、本文を歩かない。
// document は見せる結果（まだ無ければ空）、anchor は枠の上端に来るブロックを引く本文のバイト位置
// （anchor を含むブロックの上端を枠の上端に置くのは ui・決定 7）、notice は 1 行の知らせ。
struct PreviewFrame
{
    std::shared_ptr<const core::MarkdownDocument> document;
    core::Offset anchor;
    std::optional<core::DisplayText> notice;
};
} // namespace nenenib::application
