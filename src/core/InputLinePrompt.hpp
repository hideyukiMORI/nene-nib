#pragma once

#include <cstdint>

namespace nenenib::core
{
// 入力行の種類の閉じた一覧（ADR 0032 の決定 1）。プロンプトの文字はこの値から決まり、
// 描画はどの入力行でも 1 本の経路を通る（ARC-001）。増えたら renderer の switch が落ちる。
enum class InputLinePrompt : std::uint8_t
{
    // Ex（`:`）。
    ex,
    // Ctrl+P の設定一覧。入力行にプロンプトの文字を出さない。
    palette,
    // `/` の検索。
    search_forward,
    // `?` の検索。
    search_backward
};
} // namespace nenenib::core
