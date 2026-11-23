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
    signature_help
};
} // namespace nenenib::application
