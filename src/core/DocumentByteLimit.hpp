#pragma once

#include <cstddef>

namespace nenenib::core
{
// 開く本文と、自動退避から読み戻す本文の上限（#792・ADR 0121 の追補の決定 8）。正本はここ 1 つで、
// application の開く道と adapters の退避の読み戻しが同じ値を読む。値は ADR 0010 の決定 12 の
// 64 MiB のまま（上げるのは巨大ファイルの段 2 以降）。
inline constexpr std::size_t maximum_document_bytes = std::size_t{64} * 1024U * 1024U;
} // namespace nenenib::core
