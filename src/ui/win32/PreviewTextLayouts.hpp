#pragma once

#include "PreviewBlock.hpp"
#include "PreviewTextLayout.hpp"
#include "PreviewTextStyle.hpp"
#include "RenderFailure.hpp"

#include <cstddef>
#include <cstdint>
#include <dwrite.h>
#include <expected>
#include <optional>
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
// capacity 件までで、超えたら最も古いもの（最後に引いた通し番号が最小のもの）から捨てる。探索は
// 鍵の hash を先に比べる（#876）。構築では OS 資源を作らない。
class PreviewTextLayouts final
{
  public:
    using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
    static constexpr std::size_t capacity = 256;

    // 保持から引き、無ければ作って保持する。width は layout の幅（物理画素）。
    [[nodiscard]] std::expected<TextLayout, RenderFailure>
    get(const core::PreviewBlock &block, float width, const PreviewTextStyle &style);
    // get と同じ鍵の保持の entry（描く側が装飾の効果と行内コードの矩形を entry に置く・#876）。
    // 指す先は次の get / entry / measure / advance の呼び出しまで有効。
    [[nodiscard]] std::expected<PreviewTextLayout *, RenderFailure>
    entry(const core::PreviewBlock &block, float width, const PreviewTextStyle &style);
    // 高さ（GetMetrics().height・物理画素）だけを測る。保持に無ければ作って保持しない。
    [[nodiscard]] std::expected<float, RenderFailure>
    measure(const core::PreviewBlock &block, float width, const PreviewTextStyle &style);
    // get の layout の送りの幅（widthIncludingTrailingWhitespace・物理画素）。記号の列の幅に使う。
    [[nodiscard]] std::expected<float, RenderFailure>
    advance(const core::PreviewBlock &block, float width, const PreviewTextStyle &style);
    [[nodiscard]] std::size_t held() const noexcept;
    // 作った layout の数（get と measure の両方・試験が再利用を確かめる）。
    [[nodiscard]] std::size_t created() const noexcept;

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
         std::size_t hash);
    [[nodiscard]] HRESULT space(IDWriteTextLayout &layout, bool mono, float size, float line);
    [[nodiscard]] std::optional<std::pair<float, float>> natural(bool mono, float size);
    [[nodiscard]] HRESULT decorate(IDWriteTextLayout &layout, const core::PreviewBlock &block,
                                   std::string_view shown);
    [[nodiscard]] std::vector<PreviewTextLayout>::iterator find(const core::PreviewBlock &block,
                                                                float width,
                                                                const PreviewTextStyle &style,
                                                                std::size_t hash);

    Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
    std::optional<PreviewTextStyle> style_;
    TextFormat body_;
    TextFormat code_;
    std::wstring code_family_;
    std::vector<Spacing> spacings_;
    std::vector<PreviewTextLayout> held_;
    std::size_t created_ = 0;
    std::uint64_t clock_ = 0;
};
} // namespace nenenib::ui::win32
