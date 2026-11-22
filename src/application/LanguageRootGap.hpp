#pragma once

#include <cstdint>

namespace nenenib::application
{
// アクティブな文書の root がまだ決まらない理由（ADR 0161 の 2026-11-22 の追補・段 S3）。untitled は
// パスも設定の root も無い（段 S2 のまま同じ言語のどの席も受ける・子は起こさない）、seeking は印を
// 探す答えを待っている（どの席も受けず、子も起こさない）。
enum class LanguageRootGap : std::uint8_t
{
    untitled,
    seeking
};
} // namespace nenenib::application
