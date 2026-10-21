#pragma once

#include "MarkdownDocument.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "RenderFailure.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
// プレビューの見えている要素の縦の位置と字下げ（ADR 0151 の決定 16・17・ADR 0155 の決定 7）。
// 縦は枠の上端からの物理画素で、要素の間を含む。枠の上端は錨の要素（viewport.first）の上端で、
// 最初の要素のときだけ上の余白を空ける。錨の要素から順に高さ（layout の GetMetrics().height）を
// 足し、枠の高さを超えたら止める（錨より前と枠の下の要素は測らない・#811 のレビュー R1）。
// 字下げは枠の左端からの物理画素で、リストの中の要素は同じリストの深さごとの記号の列
// （最長の記号の幅＋8 DIP と 24 DIP の大きい方・記号は深さと文字の長さごとに 1 つだけ測る）を足す。
// リストの区切りと記号の列は測る範囲の中だけで決め、読むのは錨の 1 つ前と測る範囲の要素だけ
// （#813・ADR 0155 の追補）。後の項目で列が広がれば、錨から置き直す（広がるのは記号の組ごとに
// 1 回だけなので止まる）。
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
    // measure が読んだ要素の数（錨の 1 つ前と、置き直しを含めて置こうとした要素・#813 の契約）。
    [[nodiscard]] std::size_t scanned() const noexcept;

  private:
    using Columns = std::vector<float>;
    // 記号の幅を測った（深さ, 記号の文字の長さ）。
    using MarkerKey = std::pair<std::size_t, std::size_t>;
    // リストの深さごとの記号の列と、測った記号の組。
    using ListColumns = std::pair<Columns, std::vector<MarkerKey>>;

    // 錨から 1 回置く。同じリストの前の要素の字下げが広がったら false（錨から置き直す）。
    [[nodiscard]] std::expected<bool, RenderFailure> place(const core::MarkdownDocument &document,
                                                           PreviewTextLayouts &layouts,
                                                           const PreviewTextStyle &style,
                                                           const PreviewViewport &viewport);
    // index の要素が属するリストの、測る範囲の中の最初の位置（リストの外の要素は自分）。読んだ
    // ことのない要素は前から 1 つずつ足す（置き直しでは読み直さない）。
    [[nodiscard]] std::size_t start_of(const core::MarkdownDocument &document, std::size_t index);
    // index の要素の前の間（錨の要素は 0）。
    [[nodiscard]] float gap_before(const core::MarkdownDocument &document, std::size_t index,
                                   const PreviewTextStyle &style);
    [[nodiscard]] ListColumns &list_at(std::size_t start);
    // block の記号の幅を list の列に入れる。列が広がったら true。
    [[nodiscard]] static std::expected<bool, RenderFailure> widen(ListColumns &list,
                                                                  const core::PreviewBlock &block,
                                                                  PreviewTextLayouts &layouts,
                                                                  const PreviewTextStyle &style);

    std::size_t first_ = 0;
    // [first_, first_ + starts_.size()) の要素のリストの最初の位置と、今の深さごとの記号の種類。
    std::vector<std::size_t> starts_;
    std::vector<std::uint8_t> kinds_;
    // 測る範囲のリストの最初の位置ごとの記号の列（置き直しでも残す）。
    std::vector<std::pair<std::size_t, ListColumns>> lists_;
    std::vector<float> tops_;
    std::vector<float> bottoms_;
    std::vector<float> indents_;
};
} // namespace nenenib::ui::win32
