#pragma once

#include "BodyTextLayout.hpp"
#include "BodyTextStyle.hpp"
#include "TextLayoutPort.hpp"

#include <dwrite_3.h>
#include <expected>
#include <cstdint>
#include <optional>
#include <string>
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
    [[nodiscard]] std::expected<float, application::TextLayoutFailure>
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
    [[nodiscard]] bool ensure_factory();
    // 格子の升に字を置く（ADR 0140 の決定 5・試作）。各文字の送りを升数ぶんに揃える。
    [[nodiscard]] HRESULT space_cells(IDWriteTextLayout *layout,
                                      const application::TextLayoutRequest &request) const;
    [[nodiscard]] HRESULT make_format(const application::TextLayoutRequest &request);
    [[nodiscard]] std::wstring family(std::string_view requested) const;
    void attach_fallback();
    [[nodiscard]] HRESULT set_spacing(const application::TextLayoutRequest &request);
    [[nodiscard]] std::expected<BodyTextLayout, application::TextLayoutFailure>
    make_layout(const application::TextLayoutRequest &request);
    [[nodiscard]] TextLayout retain(BodyTextLayout entry);
    [[nodiscard]] BodyTextLayout *find(IDWriteTextLayout *layout);

    Microsoft::WRL::ComPtr<IDWriteFactory7> factory_;
    TextFormat format_;
    std::optional<BodyTextStyle> style_;
    std::vector<BodyTextLayout> current_;
    std::vector<BodyTextLayout> previous_;
    // 描画前の連続入力は最後の1行だけ。途中の変更行を蓄積しない。
    std::optional<BodyTextLayout> pending_;
    bool collecting_ = false;
    // 等幅の 1 升の控え（字体・大きさ・DPI が同じなら測り直さない）。
    std::string cell_family_;
    float cell_points_ = 0.0F;
    std::uint32_t cell_dpi_ = 0;
    float cell_ = 0.0F;
};
} // namespace nenenib::ui::win32
