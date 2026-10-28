#pragma once

#include "FilePath.hpp"
#include "SettingsVersion.hpp"

namespace nenenib::adapters::win32
{
// 版ごとの設定の経路。current は書く版（v4）、prior は v3、previous は v2、
// legacy は v1。古い版は新しい版が無いときだけ読む（ADR 0114）。
struct SettingsPaths
{
    core::FilePath current;
    core::FilePath prior;
    core::FilePath previous;
    core::FilePath legacy;
};

[[nodiscard]] const core::FilePath &settings_path(const SettingsPaths &paths,
                                                  SettingsVersion version) noexcept;
} // namespace nenenib::adapters::win32
