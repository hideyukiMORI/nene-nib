#pragma once

#include "LineNumber.hpp"
#include "VimPatternCaptures.hpp"

#include <cstddef>

namespace nenenib::core
{
// 置換の範囲と仕方（ADR 0168 の決定 2・5）。first から last の行（両端を含む・本文の中）を走り、
// every なら行の当たりを全部（`g`）、偽なら行の最初の 1 つだけを置き換える。count_only は数える
// だけで Edit を作らない（`n`）。limit は置き換えた後の文書の上限（製品は maximum_document_bytes・
// 試験だけが小さな値を置く・#1376）。どの値も単独で妥当なので公開 aggregate（CPP-003）。
struct SubstituteRequest
{
    LineNumber first;
    LineNumber last;
    bool every;
    bool count_only;
    std::size_t limit;
    // 群の範囲を引く道の升の数の上限（製品は vim_capture_cell_limit・試験だけが小さな値を置く・
    // ADR 0168 の追補 13）。超える当たりがあれば置換全体を too_complex で断る。
    std::size_t capture_limit = vim_capture_cell_limit;
};
} // namespace nenenib::core
