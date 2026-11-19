#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// 参照の先頭の scheme
// の唯一の判定（ARC-012・#1869）。ローカルの参照の解決（resolve_local_reference）
// とリンクの種類（link_destination）が共用する。OS とロケールに触れない。

// text の先頭の scheme（英字で始まり英数字と `+` `-` `.` が続いて `:` で終わる・2 文字以上）の
// `:` の前までの長さ。1 文字（`C:`）はドライブの文字なので scheme ではない。無ければ nullopt。
[[nodiscard]] std::optional<std::size_t> uri_scheme_length(std::string_view text) noexcept;
} // namespace nenenib::core
