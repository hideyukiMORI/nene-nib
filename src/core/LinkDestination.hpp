#pragma once

#include "FilePath.hpp"
#include "LinkKind.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// リンクの宛先の種類と見せる綴り（ADR 0151 の追補 2026-11-19・#1869）。
// text は web と mail では OS に渡す ASCII の URI（非 ASCII の字は `%XX`）、
// document では解いた経路、heading では `#` の後ろ、refused では元の綴り。
// document は OS へ渡さず、Nib の open_document の 1 本でだけ開く。
// 公開 aggregate で、比較は非メンバー（CPP-003）。
struct LinkDestination
{
    LinkKind kind;
    std::string text;
};

[[nodiscard]] inline bool operator==(const LinkDestination &left, const LinkDestination &right)
{
    return left.kind == right.kind && left.text == right.text;
}

// リンクの宛先 target（PreviewLink の target）の種類を決める唯一の純関数。
// document は開いている Markdown のファイルの経路（無題なら nullopt）。
// ファイルと OS に触れない（ARC-007）。
// - 空・前後の空白・制御文字・不正な UTF-8 → refused。
// - `#` で始まる → heading。
// - scheme（uri_scheme_length・1 文字はドライブ）は ASCII の大小を畳んで見る。
// - http / https は `//` と空でない ASCII の host を求めて web。
//   authority に `%` か `@` があれば refused。
// - mailto は引数を subject / body / cc / bcc だけ受けて mail。
//   値の中の 2 つ目の生の `?` は refused。
// - web と mail は RFC 3986 に無い ASCII か不完全な `%` があれば refused。
// - ほかの scheme → refused。
// - scheme の無い宛先と file: は resolve_local_reference（画像と同じ規則）で解く。
//   拒否・見えない字（双方向の制御・C1 制御・幅 0 の字）・装置の予約名・
//   `.` か空白で終わる名前・テキストでない拡張子（folder_lists）は refused、
//   ほかは document。
[[nodiscard]] LinkDestination link_destination(std::string_view target,
                                               const std::optional<FilePath> &document);
} // namespace nenenib::core
