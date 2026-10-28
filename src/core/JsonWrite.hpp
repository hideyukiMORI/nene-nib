#pragma once

#include "JsonValue.hpp"

#include <string>

namespace nenenib::core
{
// JsonValue を JSON の本文にする唯一の出口（ADR 0160 の決定 4）。OS に触れない純関数。
// 最小の形（空白なし）で、object は持っている並びのまま（重複する鍵もそのまま）書く。文字列は
// `"` と `\` と制御文字（U+0000〜U+001F）だけを escape し、非 ASCII は UTF-8 のまま写す。
// 数は integer を持てば整数で、持たなければ std::to_chars の往復できる最短の形で書く。
// NaN と無限は JSON に無いので `null` と書く（parse_json は作らない）。
[[nodiscard]] std::string write_json(const JsonValue &value);
} // namespace nenenib::core
