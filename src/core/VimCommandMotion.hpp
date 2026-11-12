#pragma once

#include "ExLineRequest.hpp"
#include "Offset.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// オペレータ待ちの `:`（`d:` `c:` `y:` など）の確定（ADR 0022 の追補 #1549・Vim の nv_colon）。Vim
// は Ex の命令が動かしたキャレットまでを文字単位の exclusive な移動としてオペレータに渡す。typed は
// 入力行に打った本文（`.` の記録に `:` の後ろの鍵として積む・Vim の repeat_cmdline）。line は命令が
// 行へ跳ぶ形（`:N` `:$`・`:.` `:.,.+N` は controller がキャレットの行から写した行）の行き先で、
// 空ならキャレットは動かない（`:set` `:noh` など・空の範囲のオペレータ）。
// origin は命令が先に本文を書き換えてキャレットを動かした形（`d:s/a/b/`・ADR 0168 の追補 16）の
// 命令の前のキャレットで、範囲はそこから今のキャレットまで。空なら今のキャレットから。
struct VimCommandMotion
{
    std::string typed;
    std::optional<ExLineRequest> line;
    std::optional<Offset> origin = std::nullopt;
};
} // namespace nenenib::core
