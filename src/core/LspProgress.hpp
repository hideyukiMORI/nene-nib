#pragma once

#include "DisplayText.hpp"
#include "LspNotification.hpp"
#include "LspProgressValue.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// 進み具合の method（LSP 3.17・ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。create はサーバーからの
// 要求で、Nib は result: null で答える（`initialize` で `window.workDoneProgress: true`
// を申し出る）。
inline constexpr std::string_view lsp_progress_method = "$/progress";
inline constexpr std::string_view lsp_progress_create_method = "window/workDoneProgress/create";

// `$/progress` の通知 → 1 通の値。params の token（整数か文字列）と value の kind（begin・report・
// end）が読めるときだけ値を持つ。title・message は文字列のとき、percentage は 0 以上の整数のとき
// だけ値を持ち（100 を超える数は 100）、ほかの形の欄は欠けたものとして読む。value の kind が
// 知らない文字列（WorkDoneProgress でない部分の結果など）・ほかの method は値なし。
[[nodiscard]] std::optional<LspProgressValue> lsp_progress_of(const LspNotification &notification);

// ステータスバーの言語サーバーの欄の 1 行（追補）。「<言語>: <題> <文> <百分率>%」で、無い欄は
// 間の空白ごと書かない。notice_line と同じく最初の改行の前まで・制御文字は空白・256 バイトで切る。
// 題も文も百分率も無ければ値なし。
[[nodiscard]] std::optional<DisplayText> lsp_progress_text(std::string_view language,
                                                           const LspProgressValue &progress);
} // namespace nenenib::core
