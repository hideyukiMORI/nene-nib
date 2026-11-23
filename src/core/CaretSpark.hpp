#pragma once

namespace nenenib::core
{
// 打ったときの粒の 1 つ（ADR 0192 の追補・#1979）。x と y は打った所（粒の出どころ）からのずれ、
// size は四角の一辺（どれも DIP・描く側が物理画素へ写す）、opacity は 0〜1 の不透明度、tinted は 2
// つ目の色（尾の先と同じ ime）で描くか（偽なら accent）。公開 aggregate（ADR 0007）。
struct CaretSpark
{
    float x;
    float y;
    float size;
    float opacity;
    bool tinted;
};
} // namespace nenenib::core
