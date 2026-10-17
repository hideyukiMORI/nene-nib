#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <variant>

namespace nenenib::core
{
// 行の中の画面行を問う（ADR 0144 の決定 2）。target が Offset なら「その位置は行 line の何番目の
// 画面行か」、std::size_t なら「行 line の その番目（0 始まり）の画面行はどこからどこまでか」。
// engine が VimStep.row_request に立て、controller が VimRowSpan で答えて同じ鍵を流し直す。
struct VimRowRequest
{
    LineNumber line;
    std::variant<Offset, std::size_t> target;
};
} // namespace nenenib::core
