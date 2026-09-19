#pragma once

#include "AppearancePort.hpp"
#include "ClipboardPort.hpp"
#include "CodePagePort.hpp"
#include "FilePort.hpp"
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
};
} // namespace nenenib::application
