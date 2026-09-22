#pragma once

#include <cstdint>

namespace nenenib::core
{
// 1 つの code point が画面で占める桁の数（ADR 0034 の決定 1）。Vim の意味論のためだけの値で、
// DirectWrite が決める画素の幅とは別物である。固定 Vim 9.1 の `strdisplaywidth()` を全 code point
// で実測した 4 種類と、C1 制御文字を測り直した 1 種類だけが現れる（`ambiwidth=single`）。
//
// zero        … 結合文字など、直前の文字に重なって桁を増やさないもの
// single      … 1 桁
// wide        … 2 桁（East Asian Width の W / F と、Vim が `^X` と描く制御文字）
// unprintable … 6 桁。Vim が `<200b>` の形で描く書式用文字（U+200B・U+FEFF 等）
// hex         … 4 桁。Vim が `<85>` の形で描く C1 制御文字（U+0080〜U+009F・Issue #147）。
//               固定 Vim 9.1 の実測 `strdisplaywidth("\x85") == 4`
//
// Tab はここに載せない。次の tabstop までという規則は桁の位置で決まるので `virtual_column` が持つ。
enum class DisplayWidth : std::uint8_t
{
    zero,
    single,
    wide,
    unprintable,
    hex
};
} // namespace nenenib::core
