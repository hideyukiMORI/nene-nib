#pragma once

#include "FileFailure.hpp"
#include "FilePath.hpp"

#include <windows.h>

#include <shellapi.h>

#include <expected>
#include <vector>

namespace nenenib::ui::win32
{
// HDROP を引き取り、返る前に解放する。実ファイルには触れない（ADR 0116）。
[[nodiscard]] std::vector<std::expected<core::FilePath, application::FileFailure>>
take_dropped_files(HDROP drop);
} // namespace nenenib::ui::win32
