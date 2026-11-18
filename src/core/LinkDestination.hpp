#pragma once

#include "FilePath.hpp"
#include "LinkKind.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// リンクの宛先の種類と見せる綴り（ADR 0151 の追補 2026-11-19・#1869）。text は web と mail では元の
// 綴り（確かめで見せ、OS に渡す綴り）、document では解いた経路、heading では `#` の後ろ、refused
// では 元の綴り。公開 aggregate で、比較は非メンバー（CPP-003）。
struct LinkDestination
{
    LinkKind kind;
    std::string text;
};

[[nodiscard]] inline bool operator==(const LinkDestination &left, const LinkDestination &right)
{
    return left.kind == right.kind && left.text == right.text;
}

// リンクの宛先 target（PreviewLink の target）の種類を決める唯一の純関数。document は開いている
// Markdown のファイルの経路（無題なら nullopt）。ファイルと OS に触れない（ARC-007）。
// - 空・制御文字（U+0000〜U+001F・U+007F）を含む → refused。
// - `#` で始まる → heading。
// - scheme（uri_scheme_length・1 文字はドライブ）を ASCII の大小を畳んで見る。http / https → web・
//   mailto → mail・file → ローカルの参照・ほかの scheme → refused。
// - scheme の無い宛先と file: は resolve_local_reference（画像と同じ規則・ADR 0155 の追補
// 48）で解き、
//   拒否は refused。解いた経路の名前が同じフォルダの一覧に出ない拡張子（folder_lists・D34）なら
//   refused、ほかは document。
[[nodiscard]] LinkDestination link_destination(std::string_view target,
                                               const std::optional<FilePath> &document);
} // namespace nenenib::core
