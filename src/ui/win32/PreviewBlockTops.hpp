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
// プレビューの要素ごとの縦の位置（ADR 0151 の決定 16）。枠の上端からの物理画素で、上の余白と
// 要素の間を含む。高さは layout の GetMetrics().height を上から足した部分和で、見えている
// 要素は二分探索で引く。文書・幅・字体が変われば作り直す（今は呼び手が毎回作る・段 5 で
// 差し替えのたび）。測るときに layout を保持しない（PreviewTextLayouts::measure）。
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
    [[nodiscard]] std::size_t size() const noexcept;
    // 最後の要素の下端に下の余白（上と同じ）を足した全体の高さ。
    [[nodiscard]] float total() const noexcept;

  private:
    std::vector<float> tops_;
    std::vector<float> bottoms_;
    float total_ = 0;
};
} // namespace nenenib::ui::win32
