#pragma once

#include "JsonFailure.hpp"
#include "JsonValue.hpp"

#include <cstddef>
#include <expected>
#include <string_view>

namespace nenenib::core
{
// 配列と object の入れ子の上限（ADR 0160 の決定 4）。超える入力は too_deep で断る。
inline constexpr std::size_t json_depth_limit = 128;

// RFC 8259 の JSON の本文を値にする唯一の入口（ADR 0160 の決定 4）。OS に触れない純関数。
// 入力は全体が正しい UTF-8 でなければならない。先頭と末尾の空白（SP・HT・LF・CR）を許し、値の後ろに
// 残るほかのバイトは trailing_characters。`\uXXXX` と代理対を UTF-8 へ写す。数は std::from_chars で
// 読み（ロケールに依らない・ARC-007）、double の範囲を外れる数（桁あふれと桁落ちの両方）は
// number_out_of_range。重複する鍵はそのまま並べて持つ（読むときは後勝ち・json_member）。
[[nodiscard]] std::expected<JsonValue, JsonFailure> parse_json(std::string_view text);
} // namespace nenenib::core
