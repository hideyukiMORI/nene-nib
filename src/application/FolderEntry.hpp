#pragma once

#include "FilePath.hpp"

namespace nenenib::application
{
// フォルダの直下の 1 項目（ADR 0162 の決定 5）。path はフルパス、folder はフォルダか、hidden は
// 隠し属性か。値だけを運ぶ公開 aggregate（CPP-003・ADR 0004）。
struct FolderEntry
{
    core::FilePath path;
    bool folder;
    bool hidden;
};
} // namespace nenenib::application
