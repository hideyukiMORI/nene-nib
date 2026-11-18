#pragma once

#include "CompletionCandidate.hpp"
#include "CompletionEntry.hpp"
#include "CompletionSource.hpp"
#include "LspCompletionList.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 補完の面の候補の列を作る純関数（ADR 0190 の決定 5）。並べるのと絞るのはここの 1 か所で、
// application の CompletionSession は結果を持つだけ。

// LSP の答え → 面の候補。`sortText`（無ければ label）のバイトの順に並べ、同じ鍵は届いた順のまま
// （届いた位置を 2 番目の鍵にした std::sort・std::stable_sort は使わない・ARC-003）。
[[nodiscard]] std::vector<CompletionEntry> completion_entries_of(const LspCompletionList &list);

// 面の候補の列。LSP の候補 server（completion_entries_of の順）が先、本文の語 words（Vim の
// `complete` の順）が後。本文の語のうち LSP の候補の label か insert と同じ綴りのものは落とす
// （同じ綴りは LSP の 1 つだけ）。
[[nodiscard]] std::vector<CompletionEntry>
completion_entries(std::span<const CompletionEntry> server,
                   std::span<const CompletionCandidate> words);

// 接頭辞 prefix で絞った候補の位置（entries の中の位置・列の順のまま）。filter の前方一致で、
// ASCII の大小は区別しない（ASCII でないバイトはそのまま比べる）。空の接頭辞は全部。
[[nodiscard]] std::vector<std::size_t> completion_matches(std::span<const CompletionEntry> entries,
                                                          std::string_view prefix);

// 語の頭からキャレットまでに打った分 typed が、まだ 1
// つの接頭辞か（空か、同じ種類の語の文字だけ）。
// 語の文字でない文字・別の種類の文字を打つと偽で、面は閉じる。接頭辞の決め方は
// completion_prefix_start の 1 本（Vim の実測の形・ADR 0169 の決定 2）。
[[nodiscard]] bool completion_prefix_kept(std::string_view typed) noexcept;

// 面の行の出どころの印（muted の 1 文字・ADR 0169 の決定 4）。Vim の `complete` の旗の字に合わせ、
// 本文は `.`、他のタブは `b`、LSP は `o`（omni）。訳さない。
[[nodiscard]] std::string_view completion_source_mark(CompletionSource source) noexcept;
} // namespace nenenib::core
