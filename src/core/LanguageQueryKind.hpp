#pragma once

#include <cstdint>

namespace nenenib::core
{
// 言語サーバーへキャレットの位置で問う種類（ADR 0190 の決定 8）。Vim の `gd` `K` と操作
// 「定義へ移動」「情報を表示」が同じ値を運び、問いを出すのは application の 1 本。値が増えたら
// 写し先の switch が落ちる（CPP-002）。
enum class LanguageQueryKind : std::uint8_t
{
    definition,
    hover,
    // 参照の一覧（#1947・ADR 0191 の決定 4）。Vim の `grr` と操作「参照の一覧」。
    references,
    // 記号の一覧（#1955・ADR 0191 の決定 5）。Vim の `gO` と操作「記号の一覧」。
    symbols,
    // 引数の情報（#1959・ADR 0191 の決定 6）。操作「引数の情報を表示」だけ（Vim の鍵は無い）。
    signature_help,
    // 文書を整形・選択範囲を整形（#1993・ADR 0191 の決定 7）。操作だけ（鍵なし・Vim の `gq`
    // は回さない）。
    format_document,
    format_selection,
    // 名前を変更（#2004・ADR 0194 の決定 7）。F2 と Vim の `grn` と操作「名前を変更」。
    rename
};
} // namespace nenenib::core
