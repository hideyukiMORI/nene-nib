#pragma once

#include "Offset.hpp"

#include <memory>
#include <string>
#include <vector>

namespace nenenib::core
{
// INSERT の補完の候補の写し（#1845・ADR 0169 の決定 3・ADR 0051 の `"+` の写しと同じ形）。engine は
// 語の表にも port にも触れないので、補完を始める鍵（vim_starts_completion が向きを返す鍵）の直前に
// controller が候補の列を作って置く。words は鍵の向きの順（最初の Ctrl-n / Ctrl-p で入る語が
// 先頭）で、どの語も begin からキャレットまでの本文（元の接頭辞）と置き換える本文（LF の改行）。
// begin はキャレット以下。列は共有して持ち、状態の写しを重くしない。
struct VimCompletionOffer
{
    std::shared_ptr<const std::vector<std::string>> words;
    Offset begin;
};
} // namespace nenenib::core
