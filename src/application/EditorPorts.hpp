#pragma once

#include "AppearancePort.hpp"
#include "AutosavePort.hpp"
#include "BellPort.hpp"
#include "BookmarkPort.hpp"
#include "ClipboardPort.hpp"
#include "ClockPort.hpp"
#include "CodePagePort.hpp"
#include "DocumentReadPort.hpp"
#include "FilePort.hpp"
#include "FolderPort.hpp"
#include "FontFamilyPort.hpp"
#include "HistoryPort.hpp"
#include "LanguageServerPort.hpp"
#include "MarkdownPort.hpp"
#include "PrintPort.hpp"
#include "SessionPort.hpp"
#include "SettingsPort.hpp"
#include "TextLayoutPort.hpp"
#include "ThemePort.hpp"

namespace nenenib::application
{
// 合成ルートが結ぶ借用。具象 adapter と寿命は合成ルートが所有する（ADR 0020）。
struct EditorPorts
{
    const AppearancePort &appearance;
    ClipboardPort &clipboard;
    FilePort &files;
    CodePagePort &code_pages;
    SettingsPort &settings;
    ThemePort &themes;
    SessionPort &session;
    HistoryPort &history;
    // 同じフォルダを裏で読む（ADR 0062 の決定 10）。
    FolderPort &folders;
    BookmarkPort &bookmarks;
    TextLayoutPort &text_layouts;
    // 未保存の編集を裏で退避する（ADR 0137 の決定 4）。
    AutosavePort &autosave;
    // `:set guifont=` の補完の字体の一覧（#597・D58）。
    FontFamilyPort &fonts;
    // 入力行の確定とファイル操作の断りで鳴らす（ADR 0153 の決定 4）。
    BellPort &bell;
    // Markdown のプレビューを裏で解析する（ADR 0155 の決定 5）。
    MarkdownPort &markdown;
    // 文書を開く道の読み（ADR 0159 の決定 1）。開く道だけが呼び、ほかの読みは files のまま。
    DocumentReadPort &reads;
    // 言語サーバーと話す口（ADR 0160 の決定 1）。設定の lsp がオフなら呼ばない（決定 9）。
    LanguageServerPort &language_server;
    // 現在時刻（ADR 0175 の決定 1）。F5 の日時の挿入だけが呼ぶ（決定 4）。名前の `clock` は
    // ARC-007 の字句検査（nondeterministicNames）が adapters の外で拒むので wall_clock にする。
    ClockPort &wall_clock;
    // 印刷（ADR 0177 の決定 2）。意図 PrintDocument だけが呼ぶ。
    PrintPort &print;
};
} // namespace nenenib::application
