#pragma once

#include "PreviewBlockKind.hpp"
#include "PreviewSpan.hpp"
#include "PreviewTextStyle.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// PreviewTextLayouts が保持する 1 要素の文字組み（ADR 0151 の決定 16）。鍵は要素の文字・装飾・
// 種類・段・幅（物理画素）・字体の設定で、全部が一致するときだけ使う。hash は鍵の FNV-1a で、
// 探索は先に hash を比べ、一致した候補だけ鍵の全体を比べる（#876）。used は最後に引いた
// 通し番号（最小のものから捨てる）。height は GetMetrics().height（物理画素）。
// inked は装飾の drawing effect をかけた brush の組の印（foreground の brush・まだかけて
// いなければ nullopt）で、code_runs は行内コードの範囲の画面行ごとの hit test（原点 0,0）。
// どちらも PreviewPainter が最初に描くときに作り、印の brush が変わったときだけ作り直す
// （ADR 0155 の追補・#876）。layout と一緒に捨てる。
struct PreviewTextLayout
{
    std::string text;
    std::vector<core::PreviewSpan> spans;
    core::PreviewBlockKind kind;
    std::uint8_t level;
    float width;
    PreviewTextStyle style;
    std::size_t hash;
    std::uint64_t used;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    float height;
    std::optional<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>> inked;
    std::vector<DWRITE_HIT_TEST_METRICS> code_runs;
};
} // namespace nenenib::ui::win32
