#pragma once

#include "MarkdownDocument.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "RenderFailure.hpp"

#include <cstddef>
#include <expected>
#include <vector>

namespace nenenib::ui::win32
{
// プレビューの見えている要素の縦の位置と字下げ（ADR 0151 の決定 16・17・ADR 0155 の決定 7）。
// 縦は枠の上端からの物理画素で、要素の間を含む。枠の上端は錨の要素（viewport.first）の上端で、
// 最初の要素のときだけ上の余白を空ける。錨の要素から順に高さ（layout の GetMetrics().height）を
// 足し、枠の高さを超えたら止める（錨より前と枠の下の要素は測らない・#811 のレビュー R1）。
// 字下げは枠の左端からの物理画素で、リストの中の要素は同じリストの深さごとの記号の列
// （最長の記号の幅＋8 DIP と 24 DIP の大きい方・記号は深さと文字の長さごとに 1 つだけ測る）を足す。
// 高さを測るときに layout を保持しない（PreviewTextLayouts::measure・記号の layout は保持する）。
// 引く口の要素の番号は文書の中の番号で、測った範囲は [first(), size())。
class PreviewBlockTops final
{
  public:
    [[nodiscard]] static std::expected<PreviewBlockTops, RenderFailure>
    measure(const core::MarkdownDocument &document, PreviewTextLayouts &layouts,
            const PreviewTextStyle &style, const PreviewViewport &viewport);
    // 測った最初の要素。
    [[nodiscard]] std::size_t first() const noexcept;
    // 測った要素のうち、下端が scroll より下にある最初の要素（無ければ size()）。
    [[nodiscard]] std::size_t first_visible(float scroll) const;
    [[nodiscard]] float top(std::size_t index) const;
    [[nodiscard]] float bottom(std::size_t index) const;
    // 文字の左端の枠の左端からの字下げ。
    [[nodiscard]] float indent(std::size_t index) const;
    // 測った最後の要素の次の番号。
    [[nodiscard]] std::size_t size() const noexcept;

  private:
    std::size_t first_ = 0;
    std::vector<float> tops_;
    std::vector<float> bottoms_;
    std::vector<float> indents_;
};
} // namespace nenenib::ui::win32
