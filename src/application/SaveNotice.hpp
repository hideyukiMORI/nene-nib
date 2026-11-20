#pragma once

#include <cstdint>

namespace nenenib::application
{
// 保存の後の `textDocument/didSave`（ADR 0161 の 2026-11-20 の追補・#1897）。`textDocumentSync`
// の `save` が無い・false なら none、true か `SaveOptions` なら plain、`includeText` が true なら
// with_text（保存した本文を `text` に添える）。数の形の `textDocumentSync` は none。
enum class SaveNotice : std::uint8_t
{
    none,
    plain,
    with_text
};
} // namespace nenenib::application
