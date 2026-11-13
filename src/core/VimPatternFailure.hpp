#pragma once

#include <cstdint>

namespace nenenib::core
{
// 受け付けない検索パターンの閉じた一覧（ADR 0032 の決定 4）。未対応の構文を黙って別の意味に
// せず、この値で拒否してメッセージを出す。Vim には無い失敗なので文言も本実装のものである。
enum class VimPatternFailure : std::uint8_t
{
    // 閉じない `\(`（Vim の E54）
    group,
    // 閉じない `\%(`（Vim の E53・ADR 0184 の決定 3）
    unclosed_plain_group,
    // 対の無い `\)`（Vim の E55）
    unmatched_close,
    // 10 個目の `\(`（Vim の E872）
    too_many_groups,
    // 対の無い `\}` と、量を掛ける 1 文字の中に群の端がある形（`\(e\)` + 結合文字 + `\+`）
    quantifier,
    // `\{` の中の不正（`a\{` `a\{x}` `a\{1,2,3}`・Vim の E554・ADR 0184 の決定 2）
    brace,
    // 付く原子の無い量（先頭の `\+` `\{2}`・`\(` の直後・`\{0}` の後ろ・Vim の E866）
    misplaced,
    // 量の後ろの量（`a**` `a*\+` `a\+\+` `a\{1}\{2}`・Vim の E871）
    multi_after_multi,
    // 量を開いた後の原子の数が上限（vim_pattern_atom_limit）を超える（Vim の E363 と同じ知らせ）
    too_complex,
    // `\v` `\V` `\m` `\M`
    magic,
    // そのほかの未対応の `\`（`\%` `\_` `\@` `\&` `\z` と `\n` `\t` などの英数字、末尾の単独の
    // `\`）
    escape,
    // `~`（直前の置換の文字列）
    previous_substitute,
    // 区切りの後ろの offset（`/foo/e`）
    offset,
    // 正しい UTF-8 でない pattern（末尾で切れた多バイトの列など・#906）。製品の pattern は
    // 入力行の UTF-16 から作るので来ないが、照合器の歩きの前提を入口で守る。
    invalid_utf8
};
} // namespace nenenib::core
