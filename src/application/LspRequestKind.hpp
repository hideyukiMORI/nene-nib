#pragma once

#include <cstdint>

namespace nenenib::application
{
// controller が送って答えを待つ要求の種類（ADR 0160 の決定 5・6・ADR 0190 の決定 1）。寿命の 2 つ
// と、文脈つきの 3 つ（補完・hover・定義へ）。答えの写し先はこの種類で決める。増えたら写し先の
// switch がコンパイルで落ちる（CPP-002）。
enum class LspRequestKind : std::uint8_t
{
    initialize,
    shutdown,
    completion,
    hover,
    definition,
    // 参照（#1947・ADR 0191 の決定 1）。
    references,
    // 記号（#1955・ADR 0191 の決定 1・5）。
    document_symbol,
    // 引数の情報（#1959・ADR 0191 の決定 1・6）。
    signature_help,
    // 整形（#1993・ADR 0191 の決定 1・7）。文書の全体と選択範囲。
    formatting,
    range_formatting,
    // 名前の変更（#2004・ADR 0194 の決定 2・7）。変えられる所の問いと、名前の変更。
    prepare_rename,
    rename,
    // クイックフィックス（#2005・ADR 0194 の決定 2・8・10）。直し方の問い・`edit` を後で引く
    // resolve・ サーバーの命令の実行（答えを待つ間だけ `workspace/applyEdit` を受ける）。
    code_action,
    code_action_resolve,
    execute_command
};
} // namespace nenenib::application
