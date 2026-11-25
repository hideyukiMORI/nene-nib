#pragma once

#include "EditorSettings.hpp"
#include "SettingsPaths.hpp"
#include "SettingsVersionSixTests.hpp"
#include "Win32FileAdapter.hpp"

#include <string_view>

// 1.2.0 が書く settings.v6 の見本と、その読み書きの試験（1.2.0 は v6 を書く最初の公開版・公開で
// v6 の鍵の集合が閉じる・ADR 0170 と ADR 0193 の 2026-11-24 の追補）。NibAdapterTests.cpp の長さ
// の上限（CNF-014）のため分けた。数え方は呼び手の expect を借りる（同じ実行の件数に入る）。
namespace nenenib::tests::adapters
{
// 1.2.0 の codec が書く形の settings.v6（公開元は公開の記録に書く）。encode_settings の書式
// （version=6 から indent_guide_width までの 27 行・lsp の行・mapleader・map の行）を、v6 で足した
// 9 つの鍵が全部既定でない値・features の 2 つの名前・表に無い言語の命令の行で埋めたバイト列。
// 後の版がこれを読めることを守る。
inline constexpr std::string_view released_v6_settings =
    "version=6\ncolorscheme=ubuntu-aubergine\nfont_family=Cascadia Mono\nfont_size=13.5\n"
    "guide=off\nwrap=on\nui_language=en\nlsp=on\nexplorer=on\nroot=C:\\work\n"
    "features=minimap,indentguides\nignorecase=on\nsmartcase=on\nrelativenumber=on\n"
    "scrolloff=3\nexplorer_width=320\nautoindent=on\nfinder_preview=right\n"
    "caret_motion=on\ncaret_motion_time=200\ncaret_sparks=on\npane_resize_step=12\n"
    "pane_resize_step_vim=on\nindent_guide_style=plain\nindent_guide_color_count=6\n"
    "indent_guide_highlight_current=off\nindent_guide_width=2\n"
    "lsp.cpp.command=C:\\llvm\\clangd.exe --log=error\nlsp.later-lang.command=C:\\x\\ls.exe\n"
    "mapleader=<Space>\nmap.i.jj=!<Esc>\n";

// 1.2.0 の見本の値（v6 の 9 つの鍵が既定でない値）。
[[nodiscard]] bool released_v6_values(const core::EditorSettings &settings);
// 1.2.0 の見本の読み・書き戻し・未知の鍵と古い 2 つの名前の拒否・版の取り違え・欠けた鍵の補い。
void verify_released_v6_codec(Expect expect);
// 公開前の古い 2 つの名前（indent_guide_colors と indent_guide_active）を持つ v6 は壊れた新版として
// 読めず、v5 へ戻らず、保存を断る（呼び手が置き場を空にしてから呼ぶ）。
void verify_old_v6_names_file(Expect expect, nenenib::adapters::win32::Win32FileAdapter &files,
                              const nenenib::adapters::win32::SettingsPaths &paths);
} // namespace nenenib::tests::adapters
