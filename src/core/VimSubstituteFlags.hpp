#pragma once

#include "VimPatternCase.hpp"

#include <optional>

namespace nenenib::core
{
// `:s` のフラグ（Vim の subflags_T の段 1 の部分・ADR 0168 の決定 2）。every は `g`、count は `n`、
// error は `e` の逆（既定は真＝当たりが無ければ E486）、letter_case は `i`（ignore）か `I`
// （sensitive）で、無ければ設定の ignorecase / smartcase。`&` はこの値を保ち、無ければ既定に戻して
// から文字を当てる（Vim の ex_substitute）。値は単独で妥当なので公開 aggregate（CPP-003）。
struct VimSubstituteFlags
{
    bool every = false;
    bool count = false;
    bool error = true;
    std::optional<VimPatternCase> letter_case = std::nullopt;
};
} // namespace nenenib::core
