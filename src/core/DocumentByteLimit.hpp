#pragma once

#include <cstddef>

namespace nenenib::core
{
// 開く本文の上限（ADR 0159 の決定 7・D63）。正本はここ 1 つで、application の開く道が要求に載せる。
// UTF-8 / UTF-8 BOM はこの値まで開ける。
inline constexpr std::size_t maximum_document_bytes = std::size_t{1024} * 1024U * 1024U;
// 復号の要る本文（Shift_JIS / UTF-16）の入力のバイト数の上限と、自動退避の上限（ADR 0159 の決定 7・
// L-3・L-4）。開く道は入力のバイト数で判定し（復号の後ではない）、自動退避はこれを超える本文を
// 退避せず、退避の読み戻し（AutosaveCodec）もこの値まで読む。
inline constexpr std::size_t maximum_decoded_document_bytes = std::size_t{64} * 1024U * 1024U;
// 退避の本文を符号化した後の最大（読み戻しの上限・ADR 0159 の決定 7・L-4）。計画は UTF-8 の本文の
// バイト数で maximum_decoded_document_bytes まで書くので、読み戻しはその本文を最も膨らむ符号化
// （UTF-16 は 1 バイトが 2 バイト）にして BOM（最大 3 バイト）を足した大きさまで読む。
inline constexpr std::size_t maximum_encoded_autosave_bytes =
    maximum_decoded_document_bytes * 2U + 3U;
} // namespace nenenib::core
