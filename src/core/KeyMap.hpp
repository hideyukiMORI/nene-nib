#pragma once

#include "EditorSettings.hpp"
#include "KeyMapEmit.hpp"
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
#include <utility>
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

// 今の Vim の状態で、次に読む鍵 front から表を引くモード（決定 5）。1 鍵の引数を待つ間（`f` `r`
// `m` `"` `g` `z` の次の鍵・テキストオブジェクトの `i` `a` の次）は無し（Vim
// と同じく表を引かない）。 front が回数の続きの `0`（vim_continues_count）のときも無し（Vim の
// no_zero_mapping・`nmap 0 G` があっても `50G` は 50 行目・ADR 0176 の追補 30・#1311）。
[[nodiscard]] std::optional<VimMapMode> vim_map_mode_of(const VimState &state,
                                                        const VimKey &front) noexcept;

// 表を 1 鍵ぶん引く 1 本（決定 2・4）。pending は待ちの列（前の答えが Pending なら積んだ鍵・
// Emit なら waiting）。表が空なら分岐 1 つで Pass。右辺は生のまま返し、1 段の引き直しは
// 呼び手が流す時点のモードで rhs_unit_step に行わせる（KeyMapEmit.remaps・#1276）。
[[nodiscard]] KeyMapStep key_map_step(const KeyMap &map, VimMapMode mode,
                                      std::span<const VimKey> pending, const VimKey &key);
// 待ちの列を待たずに流す（1 秒の待ちの意図 MapTimeout・決定 4）。左辺に一致する最も長い行が
// あれば右辺に替え、無い鍵はそのまま。表に一致が無ければ keys は pending と同じ列。waiting は
// 空で、ends と reads は key_map_step と同じ単位の区切り（#1257・#1266）。
[[nodiscard]] KeyMapEmit key_map_flush(const KeyMap &map, VimMapMode mode,
                                       std::span<const VimKey> pending);
// `map` の右辺の残り rest（KeyMapEmit.remaps から単位の終わりまで）の先頭の 1 つを、流す時点の
// モード mode で 1 段だけ引く（Vim は右辺の鍵も読む時点の State で引く・ADR 0176 の追補 26・
// #1276）。左辺が最も長い行が rest の中で当たればその左辺の長さと右辺（引き直さない）、当たらない
// か mode が無い（1 鍵の引数待ち・入力行・回数の続きの `0`）なら 1 と rest の先頭の 1
// 鍵。待たない。返す右辺は表の 中を指すので、呼び手は表を作り直しうる鍵を流す前に写す。
[[nodiscard]] std::pair<std::size_t, std::span<const VimKey>>
rhs_unit_step(const KeyMap &map, std::optional<VimMapMode> mode, std::span<const VimKey> rest);
} // namespace nenenib::core
