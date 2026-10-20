#pragma once

#include "MarkdownDocument.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "RenderFailure.hpp"

#include <cstddef>
#include <expected>
#include <vector>

namespace nenenib::ui::win32
{
// プレビューの要素ごとの縦の位置と字下げ（ADR 0151 の決定 16・17）。縦は枠の上端からの物理画素で、
// 上の余白と要素の間を含む。高さは layout の GetMetrics().height を上から足した部分和で、
// 見えている要素は二分探索で引く。字下げは枠の左端からの物理画素で、リストの中の要素は
// 同じリストの深さごとの記号の列（最大の記号の幅＋8 DIP と 24 DIP の大きい方）を足す。
// 文書・幅・字体が変われば作り直す（今は呼び手が毎回作る・段 5 で差し替えのたび）。
// 高さを測るときに layout を保持しない（PreviewTextLayouts::measure・記号の layout は保持する）。
class PreviewBlockTops final
{
  public:
    [[nodiscard]] static std::expected<PreviewBlockTops, RenderFailure>
    measure(const core::MarkdownDocument &document, PreviewTextLayouts &layouts,
            const PreviewTextStyle &style, float frame_width);
    // 下端が scroll より下にある最初の要素（無ければ size()）。
    [[nodiscard]] std::size_t first_visible(float scroll) const;
    [[nodiscard]] float top(std::size_t index) const;
    [[nodiscard]] float bottom(std::size_t index) const;
    // 文字の左端の枠の左端からの字下げ。
    [[nodiscard]] float indent(std::size_t index) const;
    [[nodiscard]] std::size_t size() const noexcept;
    // 最後の要素の下端に下の余白（上と同じ）を足した全体の高さ。
    [[nodiscard]] float total() const noexcept;

  private:
    std::vector<float> tops_;
    std::vector<float> bottoms_;
    std::vector<float> indents_;
    float total_ = 0;
};
} // namespace nenenib::ui::win32
