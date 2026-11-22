#pragma once

#include "SettingsVersionSixTests.hpp"

#include <string_view>

// settings の `lsp.<language>.command` の行の codec の試験。NibAdapterTests.cpp の長さの上限
// （CNF-014）のため分けた。数え方は呼び手の expect を借りる。見本の bytes（1.0.0 の v4 と v3）は
// 呼び手の表から渡す。
namespace nenenib::tests::adapters
{
void verify_settings_lsp_commands(Expect expect, std::string_view released_v4_settings,
                                  std::string_view v3_settings);
} // namespace nenenib::tests::adapters
