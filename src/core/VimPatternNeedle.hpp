#pragma once

#include <string>

namespace nenenib::core
{
// パターンが行に当たるなら、その行に必ず現れるバイト列（VimPattern::required_bytes・#1003）。
// 当たりの行を絞り込むためだけに使い、当たりの位置は照合器が決める。ascii_folded が真なら bytes
// は ASCII を小文字に畳んだ形で、行の側も ASCII を畳んで比べる（ASCII の外のバイトは畳まない）。
struct VimPatternNeedle
{
    std::string bytes;
    bool ascii_folded;
};
} // namespace nenenib::core
