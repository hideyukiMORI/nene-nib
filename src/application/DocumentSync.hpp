#pragma once

#include <cstdint>

namespace nenenib::application
{
// サーバーが求める文書の同期の種類（LSP の `TextDocumentSyncKind`・ADR 0161 の 2026-11-20 の
// 追補・#1897）。`initialize` の答えの `textDocumentSync`（数か `TextDocumentSyncOptions` の
// `change`）を読み、無い・知らない値は仕様の既定の none。none は `didChange` を送らない、full は
// 全文を 1 つの変更で、incremental は編集ごとの range で送る。
enum class DocumentSync : std::uint8_t
{
    none,
    full,
    incremental
};
} // namespace nenenib::application
