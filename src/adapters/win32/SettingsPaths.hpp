#pragma once

#include "FilePath.hpp"
#include "SettingsVersion.hpp"

namespace nenenib::adapters::win32
{
// 版ごとの設定の経路。current は書く版（v6）、shipped は 1.1.0 が読み書きする v5、released は 1.0.0
// が 読み書きする v4、prior は v3、previous は v2、legacy は v1。古い版は新しい版が無いときだけ読む
// （ADR 0114・ADR 0170 の 2026-11-16 の追補・ADR 0191 の決定 1）。
struct SettingsPaths
{
    core::FilePath current;
    core::FilePath shipped;
    core::FilePath released;
    core::FilePath prior;
    core::FilePath previous;
    core::FilePath legacy;
};

[[nodiscard]] const core::FilePath &settings_path(const SettingsPaths &paths,
                                                  SettingsVersion version) noexcept;
} // namespace nenenib::adapters::win32
