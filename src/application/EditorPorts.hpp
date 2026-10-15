#pragma once

#include "AppearancePort.hpp"
#include "BookmarkPort.hpp"
#include "ClipboardPort.hpp"
#include "CodePagePort.hpp"
#include "FilePort.hpp"
#include "FolderPort.hpp"
#include "HistoryPort.hpp"
#include "SessionPort.hpp"
#include "SettingsPort.hpp"
#include "TextLayoutPort.hpp"
#include "ThemePort.hpp"
#include "simd/AsciiNewlineScan.hpp"

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
    // 開いた本文の検証と改行の索引の走査。合成ルートが CPU から 1 回選ぶ（ADR 0006 の決定 2）。
    core::simd::AsciiNewlineScan ascii_scan = core::simd::AsciiNewlineScan::scalar;
};
} // namespace nenenib::application
