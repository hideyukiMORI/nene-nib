#pragma once

#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewFrame.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "RenderFailure.hpp"
#include "Theme.hpp"

#include <cstddef>
#include <d2d1.h>
#include <expected>
#include <memory>
#include <optional>

namespace nenenib::ui::win32
{
// other の枠の Markdown プレビュー（ADR 0155 の決定 7・11）。renderer が 1 つ所有し、要素の
// layout の保持（PreviewTextLayouts）・役割の brush・縦の位置（PreviewBlockTops）を frame を
// またいで持つ。縦の位置は錨の要素から枠の高さまでだけを測り、文書（指す先の同一性）・字体（書式・
// DPI）・錨の要素・枠の幅と高さが変わったときだけ測り直す。BodyTextLayouts
// と違い位置解決に使わないので、application の TextLayoutPort には 載せない（ADR 0114
// の範囲外）。描く先は Direct2D の描画先（製品は device context・試験は WIC）。
class PreviewPane final
{
  public:
    // 描く先とテーマの brush を用意する（描く先かテーマの名前が変わったときだけ作り直す）。
    [[nodiscard]] std::expected<void, RenderFailure> prepare(ID2D1RenderTarget &target,
                                                             const core::Theme &theme);
    // prepare の後に、枠 area に frame の文書を描く。anchor を含む要素の上端を枠の上端に置く
    // （最初の要素なら上の余白ごと見せる）。文書が空なら地だけ塗る。知らせは描かない
    // （renderer が枠の上の 1 行に書く）。失敗は値で返す。
    [[nodiscard]] std::expected<void, RenderFailure> paint(ID2D1RenderTarget &target,
                                                           const application::PreviewFrame &frame,
                                                           const PreviewTextStyle &style,
                                                           const core::LayoutRect &area);
    // 作った layout の数（試験が frame をまたいだ再利用を確かめる）。
    [[nodiscard]] std::size_t created() const noexcept;

  private:
    // 保持の縦の位置（測り直したなら新しいもの）。
    [[nodiscard]] std::expected<const PreviewBlockTops *, RenderFailure>
    remeasure(const std::shared_ptr<const core::MarkdownDocument> &document,
              const PreviewTextStyle &style, const PreviewViewport &viewport);

    PreviewTextLayouts layouts_;
    PreviewBrushes brushes_;
    std::shared_ptr<const core::MarkdownDocument> measured_;
    std::optional<PreviewTextStyle> measured_style_;
    PreviewViewport measured_viewport_{0, 0.0F, 0.0F};
    std::optional<PreviewBlockTops> tops_;
};

// anchor を含む要素（source_begin の upper_bound − 1・anchor より前に要素が無ければ 0）。
[[nodiscard]] std::size_t preview_anchor_block(const core::MarkdownDocument &document,
                                               core::Offset anchor);
} // namespace nenenib::ui::win32
