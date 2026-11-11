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
    // プレビューのタブか（ADR 0179 の決定
    // 10）。脇の束の表示値は置いた時点のものなので偽のまま持ち、 帯の表示値を並べる tab_views
    // が状態の印から立てる。renderer は題名を斜体で描く。
    bool transient = false;
};
} // namespace nenenib::application
