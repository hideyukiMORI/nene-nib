#pragma once

#include "VimSubstituteFlags.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// 前の置換の覚え（ADR 0168 の決定 1〜3・Vim の old_sub・reg_prev_sub・subflags・spats[RE_SUBST]）。
// replacement は打ったままの前の置き換え（`:s` `:&` `&` が使う・`~` は広げる前）、previous は `~`
// を広げた後の前の置き換え（次の `~` が入れる）、flags は前のフラグ（`&` が保つ）、pattern は前の
// 置換のパターン（`:s` `:&` `&` が使う・`/` の検索では変わらない）。窓に 1 つで、VimState が
// shared_ptr で持つ（打鍵ごとに写さない）。値は単独で妥当なので公開 aggregate（CPP-003）。
struct VimSubstituteMemory
{
    std::string replacement;
    std::string previous;
    VimSubstituteFlags flags;
    std::optional<std::string> pattern;
};
} // namespace nenenib::core
