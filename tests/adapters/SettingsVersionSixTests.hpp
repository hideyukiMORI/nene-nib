#pragma once

#include "EditorSettings.hpp"

#include <string_view>

// settings.v6 の codec の試験（ADR 0192 の決定 1・#1933）。NibAdapterTests.cpp
// の長さの上限（CNF-014）のため分けた。数え方は呼び手の expect を借りる（同じ実行の件数に入る）。
namespace nenenib::tests::adapters
{
using Expect = void (*)(bool condition, const char *description);

// 1.1.0（公開元 4fcf258c）の codec が書く形の settings.v5。1.1.0 の encode_settings の書式
// （version=5 から finder_preview までの 18 行・lsp の行・mapleader・map の行）を、既定でない値と
// 表に無い言語の命令の行で埋めたバイト列。後の版がこれを読めることを守る。
inline constexpr std::string_view released_v5_settings =
    "version=5\ncolorscheme=ubuntu-aubergine\nfont_family=Cascadia Mono\nfont_size=13.5\n"
    "guide=off\nwrap=on\nui_language=en\nlsp=on\nexplorer=on\nroot=C:\\work\n"
    "features=minimap\nignorecase=on\nsmartcase=on\nrelativenumber=on\nscrolloff=3\n"
    "explorer_width=320\nautoindent=on\nfinder_preview=right\n"
    "lsp.cpp.command=C:\\llvm\\clangd.exe --log=error\nlsp.later-lang.command=C:\\x\\ls.exe\n"
    "mapleader=<Space>\nmap.i.jj=!<Esc>\n";

// 今の書く形（v6）が pane_resize_step_vim の後ろに書くインデントガイドの 4 つの既定（ADR 0193 の
// 決定 4）。v6 の形の見本を組み立てる試験が共用する。
inline constexpr std::string_view indent_guide_defaults =
    "indent_guide_style=rainbow\nindent_guide_color_count=4\nindent_guide_highlight_current=on\n"
    "indent_guide_width=4\n";

// 1.1.0 の見本の値（v6 の鍵は既定のオフと 120 ms・インデントガイドの既定）。
[[nodiscard]] bool released_v5_values(const core::EditorSettings &settings);
// v5 の読み・v6 の形への書き戻し・v6 の 2 つの鍵の往復と拒否。
void verify_settings_v6_codec(Expect expect);
// v3 の見本に v4・v5・v6 の鍵を 1 つ足すと未知の鍵として拒む（NibAdapterTests.cpp の版の試験から
// 呼ぶ・CNF-014 の長さの上限のためここに置く）。
void verify_v3_refuses_later_keys(Expect expect, std::string_view v3_settings);
} // namespace nenenib::tests::adapters
