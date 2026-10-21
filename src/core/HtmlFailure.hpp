#pragma once

#include <cstdint>

namespace nenenib::core
{
// HTML → Markdown の変換が断る理由（ADR 0157 の決定 5・7・ARC-010 / CPP-005）。断られたら貼り付けは
// 文字だけを貼って 1 行知らせる。
enum class HtmlFailure : std::uint8_t
{
    // 終わらない `<…` や `<!--`、正しくない UTF-8。
    malformed,
    // 上限（16 MiB）を超える fragment。
    too_large
};
} // namespace nenenib::core
