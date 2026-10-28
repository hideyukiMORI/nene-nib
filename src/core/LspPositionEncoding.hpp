#pragma once

#include <cstdint>

namespace nenenib::core
{
// LSP の Position の `character` の単位（LSP 3.17 の `positionEncoding`・ADR 0160 の決定 5）。
// Nib は `general.positionEncodings: ["utf-8", "utf-16"]` を申し出て、サーバーが選んだ方で写す。
enum class LspPositionEncoding : std::uint8_t
{
    // UTF-8 のバイト数
    utf8,
    // UTF-16 の単位の数（LSP の既定）
    utf16
};
} // namespace nenenib::core