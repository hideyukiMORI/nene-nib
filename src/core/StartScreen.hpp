#pragma once

#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "FilePath.hpp"
#include "GuideEntry.hpp"
#include "StartEntry.hpp"
#include "StartStep.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace nenenib::core
{
// 最近使ったファイルとブックマークの節に出す件数の上限（ADR 0172 の決定 2）。
inline constexpr std::size_t start_section_limit = 10;

// 起動画面の選べる行の列（ADR 0172 の決定 2）。最近使ったファイル（履歴の新しい順の先頭から
// start_section_limit 件）→ ブックマーク（登録順の先頭から同じ件数）→ 「はじめる」の 3 行の順。
// 履歴とブックマークがまだ読まれていなければ空の列を渡す（「はじめる」の 3 行だけになる）。
[[nodiscard]] std::vector<StartEntry> start_entries(std::span<const FilePath> recent,
                                                    std::span<const FilePath> bookmarks);

// 鍵の案内の節（ADR 0172 の決定 2 の (d)・1 列）。何でも探す（Ctrl+P）・操作の一覧（F1）・
// エクスプローラー（Ctrl+Shift+E）の順で、鍵と名前は操作の表から引く（operation_guide_entry）。
// 設定（Ctrl+,）は設定画面の操作が表に入ったときに行を足す（ADR 0172 の追補 2026-11-11）。
[[nodiscard]] std::array<GuideEntry, 3> start_key_entries(EditMode mode, DisplayLanguage language);
// 言語の無い形は日本語の文（ui の試験の形・T1e で言語の形へ寄せる）。
[[nodiscard]] std::array<GuideEntry, 3> start_key_entries(EditMode mode);

// 選びを 1 つ進めた後の位置（ADR 0172 の追補 2026-11-11 の 5）。何も選んでいなければ next は
// 最初の行、previous は最後の行から選び始める。選んでいれば前後へ動き、端では反対の端へ回る
// （Ctrl+P の面の ↑ ↓ と同じ）。行が無ければ何も選ばない。
[[nodiscard]] std::optional<std::size_t>
start_selection_stepped(std::optional<std::size_t> selected, std::size_t count,
                        StartStep step) noexcept;
} // namespace nenenib::core
