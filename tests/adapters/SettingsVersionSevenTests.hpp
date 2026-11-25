#pragma once

#include "SettingsVersionSixTests.hpp"

#include <string>
#include <string_view>

// settings.v7 の codec と置き場の試験（ADR 0193 の 2026-11-24 の追補
// 2・#1977）。NibAdapterTests.cpp の長さの上限（CNF-014）のため分けた。数え方は呼び手の expect
// を借りる（同じ実行の件数に入る）。
// windows.h（の min / max）を core の header より前に読ませないよう、置き場の 2 つの型は宣言だけ。
namespace nenenib::adapters::win32
{
class Win32FileAdapter;
struct SettingsPaths;
} // namespace nenenib::adapters::win32

namespace nenenib::tests::adapters
{
// 今の書く形（v7）が indent_guide_width の後ろに書く不可視文字の 4 つの既定（ADR 0193 の決定 4）。
// v7 の形の見本を組み立てる試験が共用する。
inline constexpr std::string_view list_defaults =
    "list_eol=on\nlist_tab=on\nlist_wide_space=on\nlist_space=trail\n";

// v6 の形の bytes を v7 の形にした bytes（版の行を 7 にし、indent_guide_width の行の後ろに
// list_defaults を足す）。
[[nodiscard]] std::string v6_as_v7(std::string_view bytes);

// v7 の codec（v6 の見本と 1.1.0 の見本を v7 の形で書き戻す・4 つの鍵の往復と拒否・欠けた鍵の
// 既定・v6 以前の拒否・版の取り違え）。ファイルに触れない。
void verify_settings_v7_codec(Expect expect);
// v7 の置き場（v7 が無いときだけ v6 を厳密に読み、設定を変えたときだけ v7 に書いて v6 を bytes の
// まま残す・壊れた v7 は v6 へ戻らず保存を断る）。v7 の無い置き場で呼ぶ（v6 と v5 は上書きする）。
void verify_settings_v7_files(Expect expect, nenenib::adapters::win32::Win32FileAdapter &files,
                              const nenenib::adapters::win32::SettingsPaths &paths);
} // namespace nenenib::tests::adapters
