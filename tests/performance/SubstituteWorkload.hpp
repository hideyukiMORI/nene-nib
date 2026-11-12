#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// `:s` の族の費用（ADR 0168 の決定 9・#1007 段 1）。10 万行（`lineNNNNNN a b a`・約 1.6 MiB）の
// 文書で `:%s/a/b/g` を engine の 1 歩（vim_substituted）で流し、行ごとの編集を本文に当てるまで。
// 置き換えの無い `:%s/zzz/b/g`（E486・全行を照合するだけ）も並べる。
enum class SubstituteWorkload : std::uint8_t
{
    all_lines_100k,
    no_match_100k
};
} // namespace nenenib::tests::performance
