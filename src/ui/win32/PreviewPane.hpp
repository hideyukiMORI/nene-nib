#pragma once

#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewFrame.hpp"
#include "PreviewPictureKey.hpp"
#include "PreviewTextLayouts.hpp"
#include "PreviewTextStyle.hpp"
#include "PreviewViewport.hpp"
#include "RenderFailure.hpp"
#include "Theme.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <d2d1_1.h>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// other の枠の Markdown プレビュー（ADR 0155 の決定 7・11）。renderer が 1 つ所有し、要素の
// layout の保持（PreviewTextLayouts）・役割の brush・縦の位置（PreviewBlockTops）を frame を
// またいで持つ。縦の位置は錨の要素から枠の高さまでだけを測り、文書（指す先の同一性）・字体（書式・
// DPI）・錨の要素・枠の幅と高さが変わったときだけ測り直す。BodyTextLayouts
// と違い位置解決に使わないので、application の TextLayoutPort には 載せない（ADR 0114
// の範囲外）。描く先は Direct2D の描画先（製品は device context・試験は WIC）。
// 描いた枠の絵は枠の大きさのオフスクリーンの bitmap に持ち、鍵（PreviewPictureKey）が前の frame と
// 同じなら DrawBitmap 1 回で枠を描く（#903・ADR 0155 の追補）。bitmap を作れない描く先では
// 今までどおり直接描く。
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
    // 最後に描いた枠の中の点 (x, y)（描画先の物理画素）を押す範囲に含むチェックボックスの要素の
    // 番号（ADR 0155 の追補
    // 24）。保持の縦の位置（PreviewBlockTops::task_box）を読むだけで測らない。
    // 描いていない・文書が空・当たらなければ nullopt。
    [[nodiscard]] std::optional<std::size_t> task_at(std::int32_t x, std::int32_t y) const;
    // 作った layout の数（試験が frame をまたいだ再利用を確かめる）。
    [[nodiscard]] std::size_t created() const noexcept;
    // 要素を描いた回数（直接か bitmap へ）・保持の絵を DrawBitmap だけで使い回した回数・作った
    // bitmap の数（試験が使い回しと作り直しを数える）。
    [[nodiscard]] std::size_t painted() const noexcept;
    [[nodiscard]] std::size_t reused() const noexcept;
    [[nodiscard]] std::size_t pictures() const noexcept;

  private:
    // 保持の縦の位置（測り直したなら新しいもの）。
    [[nodiscard]] std::expected<const PreviewBlockTops *, RenderFailure>
    remeasure(const std::shared_ptr<const core::MarkdownDocument> &document,
              const PreviewTextStyle &style, const PreviewViewport &viewport);
    // 要素を描く唯一の関数（直接の道と bitmap の道が共用する）。frame は描く先の座標の枠。
    [[nodiscard]] std::expected<void, RenderFailure> draw_scene(ID2D1RenderTarget &target,
                                                                const PreviewPictureKey &key,
                                                                const core::LayoutRect &frame);
    // 枠の大きさの描ける bitmap（大きさが同じなら今のもの）。作れなければ nullptr。
    [[nodiscard]] ID2D1Bitmap1 *picture_of(ID2D1DeviceContext &context, std::int32_t width,
                                           std::int32_t height);
    // key の絵を bitmap に描いて保持する（失敗したら保持を空にして返す）。
    [[nodiscard]] std::expected<void, RenderFailure>
    record(ID2D1DeviceContext &context, ID2D1Bitmap1 &picture, PreviewPictureKey key);

    PreviewTextLayouts layouts_;
    PreviewBrushes brushes_;
    std::shared_ptr<const core::MarkdownDocument> measured_;
    std::optional<PreviewTextStyle> measured_style_;
    PreviewViewport measured_viewport_{0, 0.0F, 0.0F};
    std::optional<PreviewBlockTops> tops_;
    // 最後に文書を描いた枠（hit test の原点）。
    std::optional<core::LayoutRect> area_;
    std::string theme_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> picture_;
    std::optional<PreviewPictureKey> pictured_;
    std::size_t painted_ = 0;
    std::size_t reused_ = 0;
    std::size_t pictures_ = 0;
};

// anchor を含む要素（source_begin の upper_bound − 1・anchor より前に要素が無ければ 0）。
[[nodiscard]] std::size_t preview_anchor_block(const core::MarkdownDocument &document,
                                               core::Offset anchor);
} // namespace nenenib::ui::win32
