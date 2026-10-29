#pragma once

#include "Edit.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "VimMarkPosition.hpp"
#include "VimMarks.hpp"
#include "VimRepeatFailure.hpp"

#include <expected>
#include <optional>

namespace nenenib::core
{
// マークを読み書きする純関数（ADR 0152）。engine は `m` と飛ぶ鍵で、controller は編集と鍵の終わりで
// 呼ぶ。マークの意味を決めるのはこの 1 か所だけ（ARC-001）。

// 本文の位置 → マークの位置（行と行頭からのバイトの桁）。
[[nodiscard]] VimMarkPosition vim_mark_position_of(const TextBuffer &text, Offset at);

// マークの位置 → 本文の位置。行が本文の外なら nullopt（Vim の E19）。桁は行の内容の終わりまで
// 寄せ、文字の途中なら文字の先頭へ（Vim の check_cursor・NORMAL の寄せは呼び手）。
[[nodiscard]] std::optional<Offset> vim_mark_offset(const TextBuffer &text,
                                                    const VimMarkPosition &position);

// 読む名前 → マーク（決定 3）。a〜z は名前つき、`'` と `` ` `` は直前の位置、`.` は最後の変更。
// ほかの名前は refused、置いていないマークは not_found（Vim の E20）。
[[nodiscard]] std::expected<VimMarkPosition, VimRepeatFailure> vim_mark_named(const VimMarks &marks,
                                                                              char32_t name);

// `m{name}` のあと（決定 3）。a〜z と `'` `` ` `` は置き、Vim が受けてもこの版が持たない名前
// （A〜Z・0〜9・`"` `[` `]` `<` `>`）は何も変えない（失敗にもしない）。それ以外は refused。
[[nodiscard]] std::expected<VimMarks, VimRepeatFailure>
vim_mark_set(const VimMarks &marks, char32_t name, const VimMarkPosition &at);

// 開いた文書のマーク（ADR 0152 の追補 #1116）。普通に起動した Vim と同じに、開いた位置（1 行目の
// 桁 0）を `m'` と同じ vim_mark_set でジャンプの列に 1 項積む。文書を開く道はどれもこれを使う。
[[nodiscard]] VimMarks vim_marks_opened();

// 飛ぶ鍵の前の位置を置く（決定 4・Vim の setpcmark）。前の値は prior_context に退ける。
[[nodiscard]] VimMarks vim_marks_jumped(const VimMarks &marks, const VimMarkPosition &from);

// 鍵 1 つの終わり（決定 5・Vim の checkpcmark）。飛ぶ鍵が置いた直前の位置が今のキャレットと同じか
// 消えていたら、退けた値へ戻す。prior_context は消す。
[[nodiscard]] VimMarks vim_marks_checked(const VimMarks &marks, const VimMarkPosition &caret);

// 編集で行がずれたマークを直す（決定 6・Vim の mark_adjust）。消えた行のマークは消し、下の行の
// マークは挿入・削除した行数ぶん動かし、行の中の編集では桁を動かさない。before は編集の前の本文。
[[nodiscard]] VimMarks marks_adjusted(const VimMarks &marks, const TextBuffer &before,
                                      const Edit &edit);

// 編集が置く最後の変更の位置（決定 7・Vim の changed_lines / changed_bytes が置く `'.`）。
// before は編集の前の本文。
[[nodiscard]] VimMarkPosition vim_last_change_of(const TextBuffer &before, const Edit &edit);
} // namespace nenenib::core
