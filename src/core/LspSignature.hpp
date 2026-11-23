#pragma once

#include <cstddef>
#include <string>

namespace nenenib::core
{
// 引数の情報の札に出す 1 つの署名（ADR 0191 の決定 3・6）。label は `SignatureInformation.label`
// （`add(int a, int b) -> int` の形）、active_begin と active_end は今の引数の label の中のバイトの
// 範囲（強める所・無ければ両方 0）。公開 aggregate（CPP-003）。
struct LspSignature
{
    std::string label;
    std::size_t active_begin;
    std::size_t active_end;
};
} // namespace nenenib::core
