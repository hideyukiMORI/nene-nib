#pragma once

#include <cstdint>

namespace nenenib::application
{
// ファイルの失敗は期待される結果である（ARC-010 / CPP-005 / ADR 0010 の決定 9）。
// 開く側と保存する側で同じ閉じた集合を使い、UI は 1 行の説明に写すだけ。
// unsupported_utf32 は開く道だけで出る（UTF-32 の BOM・ADR 0149 の決定 1・#787）。
// insufficient_memory は開く道の確保の失敗（adapter が std::bad_alloc を境界で値に変える・
// ADR 0159 の決定 6）。too_large は開く上限（1 GiB）を超えたとき、too_large_to_decode は復号の要る
// Shift_JIS / UTF-16 が入力のバイト数で core::maximum_decoded_document_bytes を超えたとき（決定
// 7）。
enum class FileFailure : std::uint8_t
{
    not_found,
    access_denied,
    unreadable,
    unwritable,
    too_large,
    too_large_to_decode,
    undecodable,
    unsupported_utf32,
    unencodable,
    already_exists,
    insufficient_memory
};
} // namespace nenenib::application
