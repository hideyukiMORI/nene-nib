#pragma once

#include "VimKey.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// 鍵の記法の名前（`<Esc>` `<Space>` `<C-v>` …）と鍵の 1 対（ADR 0054・ADR 0176 の決定 2）。
// 表は生成物 VimKeyNames.hpp の 1 つで、fixture の再生と設定の `map` の行が同じ表を読む。
// `<leader>` は表に無い（`mapleader` の鍵へ展開するのは key_map_of）。
// 名前は表のとおりの綴りだけで、大小を畳まない。
[[nodiscard]] std::optional<VimKey> vim_key_of_name(std::string_view name);
// 表に名前のある鍵の名前（`VimCharacter{U' '}` は `<Space>`）。表に無い文字と特殊鍵・検索の鍵は
// 無し（呼び手は文字ならその文字を書く）。
[[nodiscard]] std::optional<std::string_view> vim_key_name_of(const VimKey &key);
} // namespace nenenib::core
