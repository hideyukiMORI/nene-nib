#pragma once

#include <cstdint>

namespace nenenib::core
{
// 鍵が閉じた失敗（Vim のビープ）で終わった理由。too_large は `p` の回数が大きすぎたとき、
// ほかは 1 鍵の結果に添えて返し、再生（`.` と `@`）はこれを見て残りの鍵を捨てる
// （ADR 0046 の決定 3・Vim の「エラーで残りのマッピングを捨てる」）。
enum class VimRepeatFailure : std::uint8_t
{
    too_large,
    // 移動できない（行頭の `h`・最終行の `j`・範囲にならないオペレータ）。
    not_moved,
    // 見つからない（文字検索・検索・テキストオブジェクト）。
    not_found,
    // 受け付けない鍵（未定義の鍵・動作にならないオペレータの後ろ・空のレジスタ・直前が無い）。
    refused
};
} // namespace nenenib::core
