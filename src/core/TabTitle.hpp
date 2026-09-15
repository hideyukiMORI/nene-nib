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
} // namespace nenenib::core
