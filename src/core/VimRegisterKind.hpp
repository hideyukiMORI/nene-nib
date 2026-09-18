#pragma once

#include <cstdint>

namespace nenenib::core
{
// レジスタに入った本文の種類（ADR 0015 の決定 3）。Vim の getregtype の "v" と "V" にあたる。
// p が文字の後ろへ貼るか下の行へ貼るかは、鍵ではなくこの種類が決める。矩形（Ctrl-v）は次の縦切り。
enum class VimRegisterKind : std::uint8_t
{
    characters,
    lines
};
} // namespace nenenib::core
