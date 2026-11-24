#pragma once

#include "LspRequestContext.hpp"

#include <string>

namespace nenenib::application
{
// 名前の変更の入力行を開いた問い（#2004・ADR 0194 の決定 7）。context は変えられる所を問うた
// ときの文脈（Enter で頼むとき、文書と改訂番号が同じでなければ頼まない・位置は at）、initial は
// 入力行の初めの文字（空と同じ名前では頼まない）。公開 aggregate（CPP-003）。
struct RenameAsked
{
    LspRequestContext context;
    std::string initial;
};
} // namespace nenenib::application
