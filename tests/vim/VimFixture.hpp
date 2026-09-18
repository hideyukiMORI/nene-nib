#pragma once

#include <cstdint>
#include <string_view>

namespace nenenib::tests
{
// oracle が本物の Vim から測った 1 件（ADR 0012 の決定 7）。生成物は VimFixtures.hpp で、
// この型だけが手書きである。expected_text は getline(1, '$') を改行 1 つでつないだもの、
// line / column は Vim の line('.') / col('.')（column はバイト位置で 1 始まり）、
// register_text は getreg('"')（行単位の削除では末尾に改行が付く）、
// register_kind は getregtype('"')（文字単位は "v"・行単位は "V"・一度も使っていなければ空）。
struct VimFixture
{
    std::string_view name;
    std::string_view text;
    std::string_view keys;
    std::string_view expected_text;
    std::uint32_t line;
    std::uint32_t column;
    std::string_view register_text;
    std::string_view register_kind;
};
} // namespace nenenib::tests
