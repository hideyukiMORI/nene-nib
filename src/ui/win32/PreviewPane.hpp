#pragma once

#include "LayoutRect.hpp"
#include "MarkdownDocument.hpp"
#include "Offset.hpp"
#include "PreviewBitmaps.hpp"
#include "PreviewBlockTops.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewFrame.hpp"
#include "PreviewHitMode.hpp"
#include "PreviewImages.hpp"
#include "PreviewPictureKey.hpp"
#include "PreviewPoint.hpp"
#include "PreviewScrollSettled.hpp"
#include "PreviewTableBand.hpp"
#include "PreviewTableHit.hpp"
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
#include <span>
#include <string>
#include <vector>
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
// 読めた画像（ADR 0155 の追補 62・63）は、画像の状態（frame.images の指す先）が替わると錨から測り
// 直して描き直し（鍵に入る）、見えている要素の画像の bitmap を描く前に PreviewBitmaps に作る。
class PreviewPane final
{
  public:
    // 描く先とテーマの brush を用意する（描く先かテーマの名前が変わったときだけ作り直す）。
    [[nodiscard]] std::expected<void, RenderFailure> prepare(ID2D1RenderTarget &target,
                                                             const core::Theme &theme);
    // prepare の後に、枠 area に frame の文書を描く。anchor を含む要素の上端を枠の上端から
    // nudge + pending × 行の送りだけ上に置く（最初の要素なら上の余白ごと・畳んで正規化する・
    // ADR 0155 の決定 7 の 2026-11-20 の追補・#1894）。文書が空なら地だけ塗る。知らせは描かない
    // （renderer が枠の上の 1 行に書く）。失敗は値で返す。
    [[nodiscard]] std::expected<void, RenderFailure> paint(ID2D1RenderTarget &target,
                                                           const application::PreviewFrame &frame,
                                                           const PreviewTextStyle &style,
                                                           const core::LayoutRect &area);
    // プレビューを描かない frame で renderer が呼ぶ。画像の bitmap と、測った文書・画像の状態の
    // 参照を放す（閉じたプレビューの画素と bitmap を持ち続けない・ADR 0155 の追補 63）。
    void release() noexcept;
    // 最後の paint が送りを畳んで位置を変えたときの答え（変わらなければ nullopt・#1894）。ui は
    // 描いた後に読んで 1 回だけ送る。
    [[nodiscard]] const std::optional<application::PreviewScrollSettled> &settled() const noexcept;
    // 持っている画像の bitmap（試験が数える）。
    [[nodiscard]] const PreviewBitmaps &bitmaps() const noexcept;
    // 最後に描いた枠の中の点 (x, y)（描画先の物理画素）を押す範囲に含むチェックボックスの要素の
    // 番号（ADR 0155 の追補
    // 24）。保持の縦の位置（PreviewBlockTops::task_box）を読むだけで測らない。
    // 描いていない・文書が空・当たらなければ nullopt。
    [[nodiscard]] std::optional<std::size_t> task_at(std::int32_t x, std::int32_t y) const;
    // 最後に描いた枠の中の点 (x, y)（描画先の物理画素）の下の文字の位置（ADR 0155 の改訂 18）。
    // 要素は保持の縦の位置で引き、位置は要素の layout（保持から引く・無ければ作る）の HitTestPoint
    // で決める。要素の間の余白は上の要素の終わり、測った最初の要素より上はその頭、最後の要素より
    // 下はその終わり、水平線は頭。枠の外の点も縦の位置で引く（ドラッグが枠を出ても選び続ける）。
    // 描いていない・文書が空・layout を作れなければ nullopt。
    [[nodiscard]] std::optional<core::PreviewPoint> point_at(std::int32_t x, std::int32_t y);
    // 最後に描いた枠の中の点 (x, y) の真下の文字の位置（リンクの札・#1869 の段 L2）。point_at
    // と同じ layout の HitTestPoint
    // で、文字の上（isInside）のときだけその文字の始まりを返す。枠の外・要素の
    // 間の余白・水平線・文字の外は nullopt。
    [[nodiscard]] std::optional<core::PreviewPoint> character_at(std::int32_t x, std::int32_t y);
    // 最後に描いた枠の中で、要素 block の text の範囲 [begin, end) を囲む矩形（描画先の物理画素・
    // 折り返した範囲は全部の行を囲む）。描く所と同じ原点と layout の HitTestTextRange で、測って
    // いない要素・描いていないときは nullopt（リンクの札の置き場・#1869 の段 L2）。
    [[nodiscard]] std::optional<core::LayoutRect> range_box(std::size_t block, std::size_t begin,
                                                            std::size_t end);
    // 最後に描いた枠の中の点 (x, y) の下の枠より広い表（行と棒の上・横は行の原点から枠の右の余白
    // まで・#1883）。収まる表・表の外は nullopt。Shift+ホイールと横のホイールと棒の押しと載せた
    // マウスが引く。
    [[nodiscard]] std::optional<PreviewTableHit> table_at(std::int32_t x, std::int32_t y);
    // 最後に描いた枠で、通し番号 table の表の棒の溝の上の横の位置 x（つかんだ間のドラッグ・縦は
    // 見ない）。表が測った範囲に無い・収まる表なら nullopt。
    [[nodiscard]] std::optional<PreviewTableHit> table_bar(std::size_t table, std::int32_t x);
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
              const std::shared_ptr<const application::PreviewImages> &images,
              const PreviewTextStyle &style, const PreviewViewport &viewport);
    // frame の (錨の要素, nudge + pending × 行の送り) を畳んだ位置（#1894）。key は文書・字体・
    // 枠の寸法・画像の状態を埋めた鍵で、畳んだ位置を anchor と nudge（DIP）に書く。前に描いた
    // 鍵と同じで送りが無ければ畳まない（畳んだ結果は畳み直しても変わらない）。
    [[nodiscard]] std::expected<void, RenderFailure> fold(const application::PreviewFrame &frame,
                                                          PreviewPictureKey &key);
    // 要素 index を置き始める位置から次の要素の上端（最後の要素は下端）までの長さ（物理画素）。
    // 描く所と同じ PreviewBlockTops の測り方で 2 要素だけ測る。
    [[nodiscard]] std::expected<float, RenderFailure> span_of(const PreviewPictureKey &key,
                                                              std::size_t index);
    // 描いた後の答え（位置が変わったか送りがあったときだけ呼ぶ）。block は畳んだ錨の要素、nudge
    // はその DIP。
    void settle(const application::PreviewFrame &frame, std::size_t block, float nudge);
    // 畳んだ鍵 key の絵を枠 area に描く（保持の絵が同じなら写すだけ）。
    [[nodiscard]] std::expected<void, RenderFailure>
    paint_key(ID2D1RenderTarget &target, PreviewPictureKey key, const core::LayoutRect &area);
    // 囲みのコードの要素 index で枠の上端に掛かる行の要素の中の番号（ほかの要素は 0）。
    [[nodiscard]] std::size_t code_line_at_top(std::size_t index);
    // 測った要素 [tops.first(), tops.size()) の読めた画像の bitmap を target に作る（画像の状態が
    // null なら bitmap を全部放す）。
    // 作れない画像（device lost でない失敗）は飛ばし、箱は空のまま描く（ADR 0155 の追補 67）。
    [[nodiscard]] std::expected<void, RenderFailure>
    hold_bitmaps(ID2D1RenderTarget &target, const core::MarkdownDocument &document,
                 const application::PreviewImages *images, const PreviewBlockTops &tops);
    // 要素を描く唯一の関数（直接の道と bitmap の道が共用する）。frame は描く先の座標の枠。
    [[nodiscard]] std::expected<void, RenderFailure> draw_scene(ID2D1RenderTarget &target,
                                                                const PreviewPictureKey &key,
                                                                const core::LayoutRect &frame);
    // 測った要素 index の layout の中の点 point（枠の左上から）の文字の位置。tops は保持の縦の
    // 位置で、枠は最後に描いた枠（area_）。mode は引き方（PreviewHitMode）。
    [[nodiscard]] std::optional<core::PreviewPoint> hit_block(const PreviewBlockTops &tops,
                                                              std::size_t index,
                                                              D2D1_POINT_2F point,
                                                              PreviewHitMode mode);
    // 表の行 index の点 point（行の原点から・#1848）の文字の位置。列の左右の余白を含めた範囲で
    // 升を選び、描く所と同じ升の文字組みと揃えで升の中の位置を引いて、行の文字の位置に写す。
    [[nodiscard]] std::optional<core::PreviewPoint> hit_cell(std::size_t index, D2D1_POINT_2F point,
                                                             const PreviewTextStyle &style,
                                                             PreviewHitMode mode);
    // 表の行 index の text の範囲 [begin, end)
    // を囲む矩形（行の原点から）。範囲の始まりの升の中だけ。
    [[nodiscard]] std::optional<D2D1_RECT_F> cell_range_box(std::size_t index, std::size_t begin,
                                                            std::size_t end,
                                                            const PreviewTextStyle &style);
    // 文字の要素 index の text の範囲 [begin, end) を囲む矩形（要素の原点から）。
    [[nodiscard]] std::optional<D2D1_RECT_F> text_range_of(std::size_t index, std::size_t begin,
                                                           std::size_t end,
                                                           const PreviewTextStyle &style);
    // 測った表の行 index の横の寸法と、描く所と同じ横の送り（物理画素・#1883）。
    [[nodiscard]] PreviewTableBand table_band(std::size_t index, std::span<const float> columns,
                                              std::uint32_t dpi) const;
    [[nodiscard]] float table_shift(std::size_t index, std::span<const float> columns,
                                    const PreviewTextStyle &style) const;
    // 測った表の行 index の点（横は窓の x・縦は枠の上端からの local）の当たり（収まる表は
    // nullopt）。
    [[nodiscard]] std::optional<PreviewTableHit> table_hit(std::size_t index, std::int32_t x,
                                                           float local);
    // 枠の大きさの描ける bitmap（大きさが同じなら今のもの）。作れなければ nullptr。
    [[nodiscard]] ID2D1Bitmap1 *picture_of(ID2D1DeviceContext &context, std::int32_t width,
                                           std::int32_t height);
    // key の絵を bitmap に描いて保持する（失敗したら保持を空にして返す）。
    [[nodiscard]] std::expected<void, RenderFailure>
    record(ID2D1DeviceContext &context, ID2D1Bitmap1 &picture, PreviewPictureKey key);

    PreviewTextLayouts layouts_;
    PreviewBrushes brushes_;
    std::shared_ptr<const core::MarkdownDocument> measured_;
    // 測った画像の状態（弱い参照・ADR 0155 の追補 63）。
    std::weak_ptr<const application::PreviewImages> measured_images_;
    std::optional<PreviewTextStyle> measured_style_;
    PreviewViewport measured_viewport_{0, 0.0F, 0.0F};
    std::optional<PreviewBlockTops> tops_;
    // 最後に文書を描いた枠（hit test の原点）と、その frame の表の横の送りの量（#1883）。
    std::optional<core::LayoutRect> area_;
    std::vector<std::int32_t> table_scrolls_;
    std::string theme_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> picture_;
    std::optional<PreviewPictureKey> pictured_;
    PreviewBitmaps bitmaps_;
    std::optional<application::PreviewScrollSettled> settled_;
    std::size_t painted_ = 0;
    std::size_t reused_ = 0;
    std::size_t pictures_ = 0;
};

} // namespace nenenib::ui::win32
