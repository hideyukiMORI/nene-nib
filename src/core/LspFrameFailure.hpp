#pragma once

#include <cstdint>

namespace nenenib::core
{
// lsp_frames が断るバイト列の閉じた一覧（ADR 0160 の決定 3）。どれも続きを読んでも直らないので、
// adapter はサーバーを壊れたものとして扱う。
enum class LspFrameFailure : std::uint8_t
{
    // `名前: 値` の形でない見出しの行（`:` が無い・名前が空）
    malformed_header,
    // 見出しに `Content-Length` が無い
    missing_content_length,
    // `Content-Length` の値が 10 進の数でない・2 回ある
    invalid_content_length,
    // `Content-Length` が上限（16 MiB）を超える
    body_too_long,
    // 見出しの終わり（空行）が上限（8 KiB）の内に来ない
    header_too_long
};
} // namespace nenenib::core
