#pragma once

#include "DisplayText.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "SaveState.hpp"
#include "TextEncoding.hpp"

#include <optional>

namespace nenenib::application
{
// UI が写すだけの文書の表示値（ARC-011 / ADR 0010 の決定 9）。last_failure は開く・保存の意図で
// 設定され、次の意図で消える。path は窓が Ctrl+S の行き先を決めるために要る（決定 10）。
struct DocumentView
{
    core::DisplayText title;
    std::optional<core::FilePath> path;
    core::TextEncoding encoding;
    core::SaveState save_state;
    std::optional<FileFailure> last_failure;
};
} // namespace nenenib::application
