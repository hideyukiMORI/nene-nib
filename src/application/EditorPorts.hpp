#pragma once

#include "AppearancePort.hpp"
#include "ClipboardPort.hpp"
#include "CodePagePort.hpp"
#include "FilePort.hpp"
#include "FolderPort.hpp"
#include "HistoryPort.hpp"
#include "SessionPort.hpp"
#include "SettingsPort.hpp"
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
    // 同じフォルダを裏で読む（ADR 0062 の決定 10）。#271 では controller はまだ呼ばない。
    FolderPort &folders;
};
} // namespace nenenib::application
