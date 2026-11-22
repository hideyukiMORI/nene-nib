#pragma once

#include "SettingsTarget.hpp"

#include <string>

namespace nenenib::ui::win32
{
// 設定の面の左の押しの覚え（#1945 のレビュー R2）。target は押した部品、server は押した行が
// 「言語サーバー」の節の行ならその行の身元（LanguageServerRowView::language）で、ほかの部品では空。
// 押してから離すまでに探しの結果が届いて行の位置がずれても、離した位置の別の行を動かさないために
// 部品と身元の両方を比べる。
struct SettingsPress
{
    core::SettingsTarget target;
    std::string server;
};
} // namespace nenenib::ui::win32
