#pragma once

#include "BodyTextLayout.hpp"
#include "BodyTextStyle.hpp"
#include "TextLayoutPort.hpp"

#include <array>
#include <dwrite_3.h>
#include <expected>
#include <optional>
#include <vector>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 合成ルートが1つを持ち、applicationの位置解決とrendererが同じ資源を借りる。
// 構築ではOS資源を作らない。最初に文字組みが必要になったときだけ初期化する。
class BodyTextLayouts final : public application::TextLayoutPort
{
  public:
    using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
    [[nodiscard]] std::expected<std::size_t, application::TextLayoutFailure>
    rows(const application::TextLayoutRequest &request) override;
    [[nodiscard]] std::expected<core::TextLocation, application::TextLayoutFailure>
    locate(const application::TextLayoutRequest &request, core::DisplayCaret caret) override;
    [[nodiscard]] std::expected<core::DisplayCaret, application::TextLayoutFailure>
    hit(const application::TextLayoutRequest &request, core::TextLocation point) override;
    // request.form の書式の `0` の幅と行高。格子は等幅の書式（grid_font）の 1 升（ADR 0140 の
    // 決定 4〜6）、text は本文の書式の字幅で、巨大な行の窓の x の見積もりに使う（ADR 0139）。
    [[nodiscard]] std::expected<application::TextCell, application::TextLayoutFailure>
    cell(const application::TextLayoutRequest &request) override;
    [[nodiscard]] std::expected<TextLayout, application::TextLayoutFailure>
    get(const application::TextLayoutRequest &request);
    // 描画時だけ字形を保持する。位置を調べただけの画面外の行には字形を複製しない。
    [[nodiscard]] const BodyTextLayout *glyphs_of(IDWriteTextLayout *layout, float left = 0);
    void begin_frame();
    void end_frame();

  private:
    using TextFormat = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
    [[nodiscard]] std::expected<void, application::TextLayoutFailure>
    prepare(const application::TextLayoutRequest &request);
    [[nodiscard]] bool matches_style(const application::TextLayoutRequest &request) const;
    [[nodiscard]] HRESULT make_format(const application::TextLayoutRequest &request);
    [[nodiscard]] std::wstring family(std::string_view requested) const;
    [[nodiscard]] bool exists(const std::wstring &name) const;
    // 格子の等幅の字体（ADR 0140 の決定 6）。Cascadia Mono → Consolas → 本文の字体。
    [[nodiscard]] std::wstring grid_family(std::string_view requested) const;
    [[nodiscard]] HRESULT make_grid_format(const application::TextLayoutRequest &request);
    [[nodiscard]] HRESULT finish_format(IDWriteTextFormat *format,
                                        const application::TextLayoutRequest &request);
    [[nodiscard]] IDWriteTextFormat *format_of(const application::TextLayoutRequest &request);
    void attach_fallback(IDWriteTextFormat *format);
    [[nodiscard]] HRESULT set_spacing(IDWriteTextFormat *format,
                                      const application::TextLayoutRequest &request);
    // 格子の画面行の字形を升の左端へ置く。cluster ごとの送りを request.cells の升の幅に合わせる。
    [[nodiscard]] HRESULT align_cells(IDWriteTextLayout *layout, const std::wstring &wide,
                                      const application::TextLayoutRequest &request);
    [[nodiscard]] std::expected<BodyTextLayout, application::TextLayoutFailure>
    make_layout(const application::TextLayoutRequest &request);
    [[nodiscard]] TextLayout retain(BodyTextLayout entry);
    [[nodiscard]] BodyTextLayout *find(IDWriteTextLayout *layout);

    Microsoft::WRL::ComPtr<IDWriteFactory7> factory_;
    TextFormat format_;
    // 格子の等幅の書式。格子の文書を最初に描くときだけ作り、書式が変われば prepare が消す。
    TextFormat grid_format_;
    std::optional<BodyTextStyle> style_;
    std::vector<BodyTextLayout> current_;
    std::vector<BodyTextLayout> previous_;
    // 描画前の連続入力は最後の1行だけ。途中の変更行を蓄積しない。
    std::optional<BodyTextLayout> pending_;
    // 書式ごとに 1 回だけ測る 1 升（添字は DisplayForm）。書式が変われば prepare が消す。
    std::array<std::optional<application::TextCell>, 2> cells_{};
    bool collecting_ = false;
};
} // namespace nenenib::ui::win32
