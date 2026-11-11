#pragma once

#include "PreviewBlock.hpp"
#include "PreviewBrushes.hpp"
#include "PreviewInlineImage.hpp"
#include "PreviewTextLayout.hpp"
#include "PreviewTextStyle.hpp"
#include "RenderFailure.hpp"

#include <cstddef>
#include <cstdint>
#include <dwrite.h>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// Markdown プレビューの 1 要素 1 layout の保持（ADR 0151 の決定 16）。本文の BodyTextLayouts とは
// 別の資源で、本文の保持（ADR 0114）・字形の収集（ADR 0073）に入れない。書式は本文の字体
// （可変幅・折り返す）と等幅の字体（コード・D58・折らない）の 2 つ。保持は最近使った順に
// capacity 件までで、超えたら最も古いものから捨てる。構築では OS 資源を作らない。
// 保持した layout は共有の資源で、描くときに書式（drawing effect を含む）を変えない・位置を問わない
// （本文の BodyTextLayouts と同じ決まり・#1477・ADR 0155 の追補）。色と行内コードの面の矩形と画像の
// 箱の左上は entry に 1 回だけ置いて持ち、描く所（PreviewPainter）は paintable で entry を読む。
class PreviewTextLayouts final
{
  public:
    using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
    static constexpr std::size_t capacity = 256;

    // 保持から引き、無ければ作って保持する。width は layout の幅（物理画素）。inlines は
    // 要素の中で読めた画像（ADR 0155 の追補 62）で、箱の範囲を同じ大きさの inline の物にし、
    // 行の高さは字と箱の自然な高さにする（箱の無い layout は今の固定の行高）。箱は保持の鍵に
    // 入る（画素は入れない）。
    [[nodiscard]] std::expected<TextLayout, RenderFailure>
    get(const core::PreviewBlock &block, float width, const PreviewTextStyle &style,
        std::span<const PreviewInlineImage> inlines = {});
    // 描く前に色の brush を結ぶ（描く所が 1 回の描画の始めに呼ぶ）。前に結んだものと違えば（別の
    // PreviewBrushes か作り直して generation が違えば）、古い brush
    // を指す色を置いた保持を全部捨てる （破棄の 1 か所・色を置いていない保持は残す）。brushes
    // は次に結ぶまで生きている。
    void bind(const PreviewBrushes &brushes);
    // 描く口。get と同じ鍵で引き（無ければ作って保持し）、色がまだ結んだ brush で置かれていなければ
    // 1 回だけ置く（結んでいない・brush をまだ作っていない描く先では置かない）。返す entry は次に
    // この保持を呼ぶまで使える。
    [[nodiscard]] std::expected<const PreviewTextLayout *, RenderFailure>
    paintable(const core::PreviewBlock &block, float width, const PreviewTextStyle &style,
              std::span<const PreviewInlineImage> inlines);
    // range の画面行ごとの矩形（layout の原点から・HitTestTextRange）。layout を汚さない。
    // 描くたびに変わる範囲（全面プレビューの選択）だけが描く所から呼ぶ。
    [[nodiscard]] std::expected<std::vector<DWRITE_HIT_TEST_METRICS>, RenderFailure>
    hit_runs(IDWriteTextLayout &layout, DWRITE_TEXT_RANGE range);
    // 高さ（GetMetrics().height・物理画素）を測る。保持に無ければ作って保持する（初めて見える
    // 要素を measure と描く所で 2 回作らない・#1477）。
    [[nodiscard]] std::expected<float, RenderFailure>
    measure(const core::PreviewBlock &block, float width, const PreviewTextStyle &style,
            std::span<const PreviewInlineImage> inlines = {});
    // 箱のある layout の 1 行目の baseline が、固定の行高の layout（記号の layout）の 1 行目の
    // baseline からどれだけ下にあるか（物理画素・ADR 0155 の追補 68）。記号とチェックボックスを
    // 1 行目の文字に揃えるために足す。inlines が空なら 0（layout を引かない）。
    [[nodiscard]] std::expected<float, RenderFailure>
    first_line_shift(const core::PreviewBlock &block, float width, const PreviewTextStyle &style,
                     std::span<const PreviewInlineImage> inlines);
    // get の layout の送りの幅（widthIncludingTrailingWhitespace・物理画素）。記号の列の幅に使う。
    [[nodiscard]] std::expected<float, RenderFailure>
    advance(const core::PreviewBlock &block, float width, const PreviewTextStyle &style);
    [[nodiscard]] std::size_t held() const noexcept;
    // 作った layout の数（get と measure の両方・試験が再利用を確かめる）。
    [[nodiscard]] std::size_t created() const noexcept;
    // SetDrawingEffect と HitTestTextRange を呼んだ数（試験が「描き直しは layout を汚さない・位置
    // を問わない」を確かめる・#1477）。
    [[nodiscard]] std::size_t inked() const noexcept;
    [[nodiscard]] std::size_t hit_tests() const noexcept;

  private:
    using TextFormat = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
    // 等幅か・字の大きさ → 自然な行の高さ・baseline（物理画素）。
    using Spacing = std::tuple<bool, float, float, float>;
    [[nodiscard]] HRESULT prepare(const PreviewTextStyle &style);
    [[nodiscard]] HRESULT make_format(const std::wstring &family, float size, bool wrapped,
                                      TextFormat &format);
    [[nodiscard]] bool exists(const std::wstring &name) const;
    [[nodiscard]] bool monospaced(const std::wstring &name) const;
    [[nodiscard]] std::wstring code_family(const std::wstring &requested) const;
    [[nodiscard]] std::expected<PreviewTextLayout, RenderFailure>
    make(const core::PreviewBlock &block, float width, const PreviewTextStyle &style,
         std::span<const PreviewInlineImage> inlines);
    // 箱の範囲を inline の物にして、行の高さを自然な高さにする。
    [[nodiscard]] static HRESULT place(IDWriteTextLayout &layout,
                                       std::span<const PreviewInlineImage> inlines,
                                       std::uint32_t dpi);
    [[nodiscard]] HRESULT space(IDWriteTextLayout &layout, bool mono, float size, float line);
    [[nodiscard]] std::optional<std::pair<float, float>> natural(bool mono, float size);
    [[nodiscard]] HRESULT decorate(IDWriteTextLayout &layout, const core::PreviewBlock &block,
                                   std::string_view shown);
    // 行内コードの面の矩形と画像の箱の左上を entry に引いて置く（作った直後の汚れていない
    // layout）。
    [[nodiscard]] HRESULT locate(PreviewTextLayout &entry, const core::PreviewBlock &block,
                                 std::span<const PreviewInlineImage> inlines, std::uint32_t dpi);
    // 装飾の色を entry の layout に置く。
    [[nodiscard]] HRESULT ink(PreviewTextLayout &entry, const core::PreviewBlock &block,
                              const PreviewBrushes &brushes);
    // 鍵で引いて最近使った順の末尾へ回し、無ければ作って保持する。
    [[nodiscard]] std::expected<std::vector<PreviewTextLayout>::iterator, RenderFailure>
    hold(const core::PreviewBlock &block, float width, const PreviewTextStyle &style,
         std::span<const PreviewInlineImage> inlines);
    [[nodiscard]] std::vector<PreviewTextLayout>::iterator
    find(const core::PreviewBlock &block, float width, const PreviewTextStyle &style,
         std::span<const PreviewInlineImage> inlines);

    Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
    std::optional<PreviewTextStyle> style_;
    TextFormat body_;
    TextFormat code_;
    std::wstring code_family_;
    std::vector<Spacing> spacings_;
    std::vector<PreviewTextLayout> held_;
    // 結んだ brush と、保持の色を置いた brush の回（PreviewBrushes::generation）。
    const PreviewBrushes *inked_by_ = nullptr;
    std::uint64_t inked_generation_ = 0;
    std::size_t created_ = 0;
    std::size_t inked_ = 0;
    std::size_t hit_tests_ = 0;
};
} // namespace nenenib::ui::win32
