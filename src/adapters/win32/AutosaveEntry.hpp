#pragma once

#include "AutosaveId.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "TextPosition.hpp"

#include <optional>

namespace nenenib::adapters::win32
{
// 一覧 `autosave.v1` の 1 行ぶん（ADR 0137 の決定 4）。本文のファイル名は番号から決まる
// （`<番号>.body`）。値だけを運ぶ公開 aggregate（CPP-003）。
struct AutosaveEntry
{
    application::AutosaveId id;
    std::optional<core::FilePath> path;
    core::DisplayText title;
    core::TextPosition caret;
};
} // namespace nenenib::adapters::win32
