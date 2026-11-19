#pragma once

#include <cstddef>
#include <string>

namespace nenenib::core
{
// 名札を作る元の見出しの文字（ADR 0151 の追補 2026-11-19 の段 L3・#1869）。block は
// MarkdownDocument の blocks の番号（見出しの要素）、text は MarkdownBuilder がその見出しに受けた
// 文字のうち、名札に数えるものだけをつないだもの（画像は alt だけで、表示の言語で変わる枠
// preview_image_alt の前後を入れない・生の HTML のタグを入れない）。
struct HeadingSource
{
    std::size_t block;
    std::string text;
};
} // namespace nenenib::core
