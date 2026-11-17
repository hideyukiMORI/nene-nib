#pragma once

#include <cstdint>

namespace nenenib::application
{
// プレビューの窓を作れない理由（ADR 0189 の決定 4・段 P2）。どれも欄の 1 行の文になる。値が増えたら
// 写す switch が落ちる（CPP-002）。
// - too_large: 16 MiB を超える（core::project_search_file_limit と同じ上限）か、復号の要る本文が
//   復号の上限を超える。
// - binary: NUL を含む（UTF-16 の BOM のある本文は除く）。
// - unreadable: 開けない・読めない・判別できない文字コード。
// - not_local: 中身がまだ手元に無い（クラウドの置き場・FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS か
//   OFFLINE。取り寄せを起こさない）。
enum class PreviewFailure : std::uint8_t
{
    too_large,
    binary,
    unreadable,
    not_local
};
} // namespace nenenib::application
