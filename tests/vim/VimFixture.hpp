#pragma once

#include "VimChangeFixture.hpp"
#include "VimMacroFixture.hpp"
#include "VimViewportFixture.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace nenenib::tests
{
// oracle が本物の Vim から測った 1 件（ADR 0012 の決定 7）。生成物は VimFixtures.hpp で、
// この型だけが手書きである。expected_text は getline(1, '$') を改行 1 つでつないだもの、
// line / column は Vim の line('.') / col('.')（column はバイト位置で 1 始まり）、
// register_text は getreg('"')（行単位の削除では末尾に改行が付く）、
// register_kind は getregtype('"')（文字単位は "v"・行単位は "V"・一度も使っていなければ空）。
// macro は keys を流す前に置くマクロのレジスタ（録画は oracle で測れないので再生だけ）。
// change は鍵が Vim の変更と undo の単位を数えたか（ADR 0181・求めた fixture だけ）。
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
    std::optional<VimViewportFixture> viewport;
    // 先に置くマクロのレジスタ（ADR 0046 の決定 6）。無い fixture の行は書かない。
    std::optional<VimMacroFixture> macro = std::nullopt;
    // Vim の変更の印（ADR 0181）。求めた fixture の行だけが書く。
    std::optional<VimChangeFixture> change = std::nullopt;
    // fixture の settings のうち再生が流す行（改行でつなぐ・#1626）。再生は窓を置いた後・鍵の前に
    // 1 行ずつ `:` の行として打ち、Nib が断る行は試験を落とす。Nib の固定値と同じ行（oracle の
    // NIB_FIXED_SETTINGS）は生成物に載らない。設定の無い fixture の行は書かない。
    std::string_view settings = {};
};
} // namespace nenenib::tests
