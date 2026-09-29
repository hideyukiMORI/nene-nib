#pragma once

#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "SaveState.hpp"

#include <optional>

namespace nenenib::core
{
// タブと窓の題名（ADR 0010 の決定 13）。経路が無ければ「無題」、未保存なら「● 」を前に付ける。
// DisplayText の 256 バイトを超える名前は末尾を code point 境界で落として「…」を付ける。
[[nodiscard]] DisplayText tab_title_for(const std::optional<FilePath> &path, SaveState state);
// タブの一覧が題名の後ろに添える場所（ADR 0057 の決定 7）。経路から最後の区切りと名前を除いた
// 部分で、無題と区切りの無い経路は値なし。長すぎる経路は題名と同じく末尾を落として「…」を付ける。
[[nodiscard]] std::optional<DisplayText> tab_folder_for(const std::optional<FilePath> &path);
} // namespace nenenib::core
