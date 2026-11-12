#pragma once

#include "ExAddressBase.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// Ex の番地 1 つ（ADR 0168 の決定 1）。基点に offset（`+N` `-N` と基点の後の数字の和・`+` だけは
// 1）を足した行。number は基点が number のときの行番号（大きすぎる数は size_t の最大に飽和）、
// mark は基点が mark のときの名前。after_semicolon は前の番地と `;` で区切られた（基点を省いた
// 番地と `.` が前の番地の行を指す）。どの値も単独で妥当なので公開 aggregate（CPP-003）。
struct ExAddress
{
    ExAddressBase base;
    std::size_t number;
    char mark;
    std::int64_t offset;
    bool after_semicolon;
};
} // namespace nenenib::core
