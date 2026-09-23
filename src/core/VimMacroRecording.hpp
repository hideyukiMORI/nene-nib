#pragma once

#include "VimKey.hpp"

#include <vector>

namespace nenenib::core
{
// 録画中のマクロ（ADR 0046 の決定 1・2）。name は小文字の a〜z、keys は `q{name}` のあとに
// vim_step へ届いた鍵（止める `q` は入らない）。append は `q{A-Z}` で始めた追記の録画で、
// 止めたときに既存の鍵の列の末尾へ繋ぐ。
struct VimMacroRecording
{
    char name;
    std::vector<VimKey> keys;
    bool append;
};
} // namespace nenenib::core
