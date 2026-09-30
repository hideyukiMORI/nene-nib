#pragma once

#include "FileHistory.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"

#include <cstddef>

namespace nenenib::application
{
// 履歴に覚える件数の上限（ADR 0060 の決定 8）。値の正本はここ 1 つ。
inline constexpr std::size_t history_limit = 100;

// 履歴を書き換える純関数（ADR 0060 の決定 8）。同じファイルかは FilePort::same_file が OS の規則で
// 決める（core::FilePath の == では比べない）。
// path と同じファイルを除いて先頭へ置き、history_limit で後ろを切る。
[[nodiscard]] FileHistory history_recorded(FileHistory history, const core::FilePath &path,
                                           const FilePort &files);
// path と同じファイルを全部外す。残りの順は変えない。
[[nodiscard]] FileHistory history_forgotten(FileHistory history, const core::FilePath &path,
                                            const FilePort &files);
} // namespace nenenib::application
