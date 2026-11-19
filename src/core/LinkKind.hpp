#pragma once

#include <cstdint>

namespace nenenib::core
{
// リンクの宛先の種類（ADR 0151 の追補 2026-11-19・#1869・施主決定 D105）。決めるのは
// link_destination の 1 本。値が増えたら switch が落ちる（CPP-002）。
// - web: http / https。
// - mail: mailto。
// - document: ローカルのファイル（パスと file: の URL・解き方は resolve_local_reference）。
// - heading: `#` で始まる（同じ文書の見出し・段 L3）。
// - refused: 開かない（ほかの scheme・解けない参照・テキストでない拡張子・空・制御文字）。
enum class LinkKind : std::uint8_t
{
    web,
    mail,
    document,
    heading,
    refused
};
} // namespace nenenib::core
