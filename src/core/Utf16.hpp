#pragma once

#include "ByteOrder.hpp"
#include "TextFailure.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// UTF-8 ↔ UTF-16 の変換の唯一の場所（ARC-001 / ARC-012 / CPP-014）。OS を呼ばない純関数なので、
// Win32 の W 系 API・DirectWrite・WM_CHAR の境界はどれもここを通る（Issue #13）。
// UTF-8 の検証は Utf8.hpp に任せ、ここではサロゲートペアの合成と分解だけを書く。

// 正しい UTF-8 を UTF-16 へ。不正な UTF-8 は invalid_utf8 で返す（空は空）。
[[nodiscard]] std::expected<std::wstring, TextFailure> to_utf16(std::string_view utf8);

// 同じ検証とサロゲートの規則で単位数だけを測る。文字列を確保しない（ADR 0083）。
[[nodiscard]] std::expected<std::size_t, TextFailure> utf16_length(std::string_view utf8);

// 正しい UTF-16 を UTF-8 へ。対になっていないサロゲートは invalid_utf16 で返す（空は空）。
[[nodiscard]] std::expected<std::string, TextFailure> to_utf8(std::wstring_view utf16);

// UTF-16 のファイルの本文（BOM を除いたバイト列）が偶数バイトで、サロゲートが対になっているか。
// 確保しない（detect_encoding が使う・ADR 0149 の決定 1）。
[[nodiscard]] bool valid_utf16_bytes(std::string_view bytes, ByteOrder order) noexcept;

// UTF-16 のバイト列（BOM を除く）を UTF-8 へ（決定 2）。奇数バイトと対になっていないサロゲートは
// invalid_utf16。並びを直してから to_utf8 と同じ規則で写す。
[[nodiscard]] std::expected<std::string, TextFailure> utf8_of_utf16_bytes(std::string_view bytes,
                                                                          ByteOrder order);

// 正しい UTF-8 を UTF-16 のバイト列へ（BOM は付けない・決定 3）。to_utf16 の結果を並びに写す。
[[nodiscard]] std::expected<std::string, TextFailure> utf16_bytes_of_utf8(std::string_view utf8,
                                                                          ByteOrder order);
} // namespace nenenib::core
