#pragma once

#include "Edit.hpp"
#include "SubstituteFailure.hpp"
#include "SubstituteReplacement.hpp"
#include "SubstituteRequest.hpp"
#include "SubstituteResult.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"

#include <cstddef>
#include <expected>
#include <string_view>

namespace nenenib::core
{
// 範囲の行の当たりを replacement で置き換えた行ごとの編集（ADR 0168 の決定 5・ADR 0174 の決定 7
// の 1 本）。純関数で、本文・状態を持たない。行の当たりは Vim の do_sub と同じ順で拾う: 前の当たり
// の終わりから次を探し、前の当たりの終わりにぴったり続く長さ 0 の当たりは数えず 1 文字進めて
// 探し直し（`:s/x*/-/g` は "abc" を "-a-b-c" に・実測）、行の終わりに着いたら止める。every で
// なければ行の最初の 1 つだけ。当たりは行の中だけ（照合器が行をまたがない）。replacement が群
// （`\1`〜`\9`）を読むときだけ当たりごとに VimPattern::captured を引く。
// 当たりが無ければ no_match。置き換えた後の文書のバイト数（本文 - 当たりの和 + 広げた置き換えの和）
// が request.limit を超えるなら、数えるだけの 1 周目の後、置き換えた本文を作る前に too_large
// （#1376）。count_only は 1 周目だけで edits は空。
[[nodiscard]] std::expected<SubstituteResult, SubstituteFailure>
substituted(const TextBuffer &text, const VimPattern &pattern,
            const SubstituteReplacement &replacement, const SubstituteRequest &request);

// 本文の当たりをすべて replacement（literal）で置き換えた 1 回の Edit（ADR 0174 の決定 7・
// 通常モードの「すべて置換」）。substituted の全行・every と同じ当たりを、最初の当たりの始まりから
// 最後の当たりの終わりまでの 1 つの範囲に畳む（当たりの間の本文はそのまま写す・行ごとの編集の列は
// 作らない）。restore は最初の当たりの始まりで、本文へ当てる側（controller の replace）がそのときの
// キャレットで書き直す。通常モードの語は literal なので長さ 0 の当たりにならない。
[[nodiscard]] std::expected<Edit, SubstituteFailure> substituted_all(const TextBuffer &text,
                                                                     const VimPattern &pattern,
                                                                     std::string_view replacement,
                                                                     std::size_t limit);
} // namespace nenenib::core
