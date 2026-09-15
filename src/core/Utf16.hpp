#pragma once

#include "TextFailure.hpp"

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

// 正しい UTF-16 を UTF-8 へ。対になっていないサロゲートは invalid_utf16 で返す（空は空）。
[[nodiscard]] std::expected<std::string, TextFailure> to_utf8(std::wstring_view utf16);
} // namespace nenenib::core
