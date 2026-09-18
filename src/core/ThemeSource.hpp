#pragma once

#include <string_view>

namespace nenenib::core
{
// 配色の出典と許諾（ADR 0017 の決定 2）。色の値そのものは著作物ではないが、名前と配色の
// 出どころを表に残す。テーマの実装コード（Vim script・VS Code の JSON）は写さない。
// url が指す版を正とする（同じテーマでも版で値が違うことがある）。
struct ThemeSource
{
    std::string_view author;
    std::string_view license;
    std::string_view url;
};
} // namespace nenenib::core
