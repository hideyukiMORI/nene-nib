#pragma once

#include "SettingsFailure.hpp"
#include "SettingsPaths.hpp"

#include <expected>
#include <optional>
#include <string_view>

namespace nenenib::adapters::win32
{
[[nodiscard]] std::expected<SettingsPaths, application::SettingsFailure> local_settings_paths();
// 設定と同じ親（`%LOCALAPPDATA%/NeNeNib/`）の name。テーマの置き場と前回のタブの一覧と閉じた
// ファイルの履歴が使う（ADR 0024 / ADR 0059 の決定 2 / ADR 0060 の決定 8）。親を作るのは
// local_settings_paths の 1 か所だけ。
[[nodiscard]] std::optional<core::FilePath> beside_local_settings(std::string_view name);
} // namespace nenenib::adapters::win32
