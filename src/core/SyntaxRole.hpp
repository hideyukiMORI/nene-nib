#pragma once

#include <cstdint>

namespace nenenib::core
{
// 本文の 1 区間の構文の役割（#815・段 2 の ADR 0156 の決定 1）。閉じた 10 値で、SyntaxPalette
// の欄への 写しは段 2 の switch が持つ。warning は LSP の診断のために置き、字句器（段
// 1）は出さない。
enum class SyntaxRole : std::uint8_t
{
    comment,
    keyword,
    string,
    number,
    type,
    function,
    constant,
    operators,
    error,
    warning
};
} // namespace nenenib::core
