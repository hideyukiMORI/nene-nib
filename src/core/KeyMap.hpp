#pragma once

#include "EditorSettings.hpp"
#include "KeyMapFailure.hpp"
#include "KeyMapStep.hpp"
#include "KeyMapping.hpp"
#include "MapRow.hpp"
#include "VimKey.hpp"
#include "VimMapMode.hpp"
#include "VimModeSet.hpp"
#include "VimState.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// Vim の `map` の表（ADR 0176 の決定 2）。設定の行から key_map_of が作る純粋な値で、
// 同じモードと左辺の行は 1 つだけ（後の行が前の行のそのモードを上書きする）。engine は
// この表を知らず、引くのは application の入口の key_map_step だけ（決定 3）。
struct KeyMap
{
    std::vector<KeyMapping> mappings;
};

// 左辺の打鍵の上限（D72）。待ちの列はこれより 1 つ短い列までしか積まれない。
inline constexpr std::size_t key_map_lhs_limit = 3;

// 設定の 1 行 `map.<modes>.<lhs>=<rhs>`（値の先頭の `!` は noremap）を読む（決定 1）。鍵の
// 記法・モードの文字・打鍵の数を検査し、左辺と右辺は文字列のまま持つ。
[[nodiscard]] std::expected<MapRow, KeyMapFailure> parse_map_line(std::string_view key,
                                                                  std::string_view value);
// 行の値を検査して MapRow にする。parse_map_line と Ex の追加（M1b）が通る 1 本で、
// 設定に書いて読み戻せない行（unwritable）もここで断る。
[[nodiscard]] std::expected<MapRow, KeyMapFailure>
map_row_of(VimModeSet modes, std::string_view lhs, std::string_view rhs, bool remap);
// 行の書き戻し（鍵と値）。モードの文字は `n i v x o` の順。
[[nodiscard]] std::string map_line_key(const MapRow &row);
[[nodiscard]] std::string map_line_value(const MapRow &row);
// `mapleader` の値（鍵の記法で 1 打鍵・`<leader>` は書けない）。
[[nodiscard]] std::expected<VimKey, KeyMapFailure> parse_map_leader(std::string_view text);
// 設定の `maps` と `mapleader` から表を作る（`<leader>` はここで展開する）。
[[nodiscard]] std::expected<KeyMap, KeyMapFailure> key_map_of(const EditorSettings &settings);

// 今の Vim の状態で表を引くモード（決定 5）。1 鍵の引数を待つ間（`f` `r` `m` `"` `g` `z`
// の次の鍵・テキストオブジェクトの `i` `a` の次）は無し（Vim と同じく表を引かない）。
[[nodiscard]] std::optional<VimMapMode> vim_map_mode_of(const VimState &state) noexcept;

// 表を 1 鍵ぶん引く 1 本（決定 2・4）。pending は待ちの列（前の答えが Pending なら積んだ鍵・
// Emit なら waiting）。表が空なら分岐 1 つで Pass。右辺は `map` なら 1 段だけ表で引き直し
// （右辺が左辺で始まるならその分は引き直さない・Vim と同じ）、`noremap` は引き直さない。
[[nodiscard]] KeyMapStep key_map_step(const KeyMap &map, VimMapMode mode,
                                      std::span<const VimKey> pending, const VimKey &key);
// 待ちの列を待たずに流す（1 秒の待ちの意図 MapTimeout・決定 4）。左辺に一致する最も長い行が
// あれば右辺に替え、無い鍵はそのまま。表に一致が無ければ pending と同じ列。
[[nodiscard]] std::vector<VimKey> key_map_flush(const KeyMap &map, VimMapMode mode,
                                                std::span<const VimKey> pending);
} // namespace nenenib::core
