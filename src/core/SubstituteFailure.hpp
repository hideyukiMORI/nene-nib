#pragma once

#include <cstdint>

namespace nenenib::core
{
// 全件の置換（substituted_all）が Edit を作らない理由（ADR 0174 の決定 7 の追補 #1376・ARC-010 /
// CPP-005）。
enum class SubstituteFailure : std::uint8_t
{
    // 長さのある当たりが 1 つも無い。
    no_match,
    // 置き換えた後の文書が上限（製品は maximum_document_bytes・ADR 0159 の決定 7）を超える。
    // 置き換えた本文を確保する前に断る。
    too_large,
    // 群の範囲を引く道の探索が升の数の上限（vim_capture_cell_limit）を超える一致がある（ADR 0168 の
    // 追補 13）。群を黙って空にせず、置き換えた本文を作る前に断る。
    too_complex
};
} // namespace nenenib::core
