#pragma once

#include "LspPosition.hpp"
#include "LspPositionEncoding.hpp"

namespace nenenib::application
{
// 言語サーバーの定義の場所（ADR 0190 の決定 7）。at は Position、encoding はその答えを返した席の
// 位置の単位（着地先の文書の言語の席から引かない・別の言語の文書へ跳んでも答えた席の単位で写す・
// #1916 のレビュー R2）。公開 aggregate（CPP-003）。
struct LspLanding
{
    core::LspPosition at;
    core::LspPositionEncoding encoding;
};
} // namespace nenenib::application
