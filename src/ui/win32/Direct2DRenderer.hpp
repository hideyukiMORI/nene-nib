#pragma once

#include "BodyLayout.hpp"
#include "ClauseEmphasis.hpp"
#include "CommandLayout.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"
#include "LineView.hpp"
#include "PaletteLayout.hpp"
#include "RenderFailure.hpp"
#include "RgbColor.hpp"
#include "StatusBarLayout.hpp"
#include "TimingPort.hpp"
#include "TitleBarLayout.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// Phase 0 の D1 で実測した提示経路をそのまま持つ（ADR 0007）。
// D3D11 device → composition swap chain（flip・waitable）→ DirectComposition → D2D device context。
// 座標は物理画素（context の DPI は 96 に固定し、寸法は core のレイアウト純関数が掛ける）。
class Direct2DRenderer final
{
  public:
    // 起動の節目は initialize の中で打つので、計測器は create からそのまま通す（Issue #19）。
    [[nodiscard]] static std::expected<Direct2DRenderer, RenderFailure>
    create(HWND window, std::uint32_t dpi, application::TimingPort &timing);
    [[nodiscard]] std::expected<void, RenderFailure> render(const application::EditorFrame &frame);
    [[nodiscard]] std::expected<void, RenderFailure> resize(UINT width, UINT height);
    // 本文のクリックを桁へ写す唯一の経路。DirectWrite の当たり判定は描く側が持つ（ARC-011）。
    [[nodiscard]] core::Column column_at(std::string_view text, const core::BodyLayout &body,
                                         std::int32_t x);
    [[nodiscard]] std::expected<void, RenderFailure> set_dpi(std::uint32_t dpi);
    [[nodiscard]] std::expected<void, RenderFailure> set_font(const core::EditorSettings &settings);
    // 最後に描いたキャレットの物理画素。窓が IME の候補窓をその直下に置く（ADR 0014 の決定 6）。
    // 変換中は変換中のキャレット（GCS_CURSORPOS の位置）になる。
    [[nodiscard]] RECT caret_rectangle() const noexcept;

  private:
    // 待機可能オブジェクトは HANDLE なので所有を型で閉じる（CPP-016）。
    using WaitableHandle = std::unique_ptr<void, decltype(&::CloseHandle)>;
    using TextFormat = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
    using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

    Direct2DRenderer() = default;
    [[nodiscard]] std::expected<void, RenderFailure> initialize(HWND window,
                                                                application::TimingPort &timing);
    [[nodiscard]] std::expected<void, RenderFailure> create_device();
    [[nodiscard]] std::expected<void, RenderFailure> create_swap_chain(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> bind_composition(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> create_context();
    [[nodiscard]] std::expected<void, RenderFailure> create_text_formats();
    [[nodiscard]] std::expected<void, RenderFailure>
    create_body_formats(const core::EditorSettings &settings);
    void align_text_formats();
    [[nodiscard]] const wchar_t *family(const wchar_t *preferred, const wchar_t *fallback) const;
    [[nodiscard]] HRESULT make_format(const wchar_t *face, float size_dips,
                                      DWRITE_FONT_WEIGHT weight, TextFormat &format);
    [[nodiscard]] float scaled(float dips) const noexcept;
    void fill(const core::LayoutRect &area, core::RgbColor color);
    void fill_rounded(const core::LayoutRect &area, core::RgbColor color, float radius);
    void write(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area,
               core::RgbColor color);
    void draw_cross(const core::LayoutRect &box, float half, float stroke);
    void draw_title_bar(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_tab(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_caption_glyphs(const core::TitleBarLayout &layout, core::RgbColor color);
    [[nodiscard]] TextLayout layout_of(std::string_view text, const core::BodyLayout &body);
    [[nodiscard]] TextLayout text_layout(std::string_view text, IDWriteTextFormat *format,
                                         const core::LayoutRect &area);
    // 1 行の中の範囲の当たり矩形。折り返さないので数は少なく、上限を超えた分は描かない。
    [[nodiscard]] std::size_t runs_of(IDWriteTextLayout *text, const core::LayoutRect &area,
                                      DWRITE_TEXT_RANGE range,
                                      std::span<DWRITE_HIT_TEST_METRICS> runs);
    void fill_runs(IDWriteTextLayout *text, const core::LayoutRect &area, DWRITE_TEXT_RANGE range);
    // 範囲の下端に太さ thickness の帯を引く（IME の文節の下線・ADR 0014 の決定 7）。
    void underline_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                        DWRITE_TEXT_RANGE range, std::int32_t thickness);
    // 範囲の字だけを別の色で描き直す。切り抜きの中に行の layout をもう一度通す。
    void tint_runs(IDWriteTextLayout *text, const core::LayoutRect &area, DWRITE_TEXT_RANGE range,
                   core::RgbColor color);
    void draw_line_selection(const application::EditorFrame &frame, IDWriteTextLayout *text,
                             const core::LayoutRect &area, const application::LineView &line);
    void draw_bar_caret(const application::EditorFrame &frame, IDWriteTextLayout *text,
                        const core::LayoutRect &area, UINT32 position);
    void draw_block_caret(const application::EditorFrame &frame, IDWriteTextLayout *text,
                          const core::LayoutRect &area, UINT32 position);
    void draw_caret(const application::EditorFrame &frame, IDWriteTextLayout *text,
                    const core::LayoutRect &area, std::string_view line);
    // 注目文節は accent の 2 DIP の下線と selection と同じ面、他の文節は ime の 1 DIP の
    // 下線と ime の字色（ADR 0014 の決定 7・採用案 D15）。
    void draw_target_clause(const application::EditorFrame &frame, IDWriteTextLayout *text,
                            const core::LayoutRect &area, DWRITE_TEXT_RANGE range);
    void draw_other_clause(const application::EditorFrame &frame, IDWriteTextLayout *text,
                           const core::LayoutRect &area, DWRITE_TEXT_RANGE range);
    void draw_clauses(const application::EditorFrame &frame, IDWriteTextLayout *text,
                      const core::LayoutRect &area, std::string_view shown);
    // 変換中の文字列をキャレットの位置に差し込んだ 1 行。TextBuffer は触らない（ARC-004）。
    void draw_composed_line(const application::EditorFrame &frame, const core::BodyLayout &body,
                            const core::LayoutRect &area, const application::LineView &line);
    void draw_plain_line(const application::EditorFrame &frame, const core::BodyLayout &body,
                         const core::LayoutRect &area, const application::LineView &line);
    void draw_line(const application::EditorFrame &frame, const core::BodyLayout &body,
                   std::size_t index);
    void draw_body(const application::EditorFrame &frame, const core::BodyLayout &body);
    void draw_status_bar(const application::EditorFrame &frame,
                         const core::StatusBarLayout &layout);
    void draw_toggle(const application::EditorFrame &frame, const core::StatusBarLayout &layout);
    void draw_status_left(const application::EditorFrame &frame,
                          const core::StatusBarLayout &layout);
    void draw_command(const application::EditorFrame &frame, const core::LayoutRect &area);
    void draw_completions(const application::EditorFrame &frame,
                          const core::StatusBarLayout &status);
    void draw_palette(const application::EditorFrame &frame, const core::PaletteLayout &layout);
    void draw_palette_choices(const application::EditorFrame &frame,
                              const core::PaletteLayout &layout);
    void draw_palette_choice(const application::EditorFrame &frame, const core::LayoutRect &row,
                             std::size_t index);
    void draw_palette_footer(const application::EditorFrame &frame, const core::LayoutRect &area);
    [[nodiscard]] std::expected<void, RenderFailure> draw(const application::EditorFrame &frame,
                                                          ID2D1Bitmap1 *surface);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device_;
    Microsoft::WRL::ComPtr<IDXGISwapChain2> swap_chain_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> composition_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> target_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> visual_;
    Microsoft::WRL::ComPtr<ID2D1Factory7> factory_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext6> context_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<IDWriteFactory7> dwrite_;
    TextFormat tab_format_;
    TextFormat toggle_format_;
    TextFormat status_format_;
    TextFormat mode_format_;
    TextFormat command_format_;
    TextFormat gutter_format_;
    TextFormat code_format_;
    WaitableHandle latency_{nullptr, &::CloseHandle};
    RECT caret_rectangle_{};
    std::int32_t caret_width_ = 2;
    std::uint32_t dpi_ = 96;
    // DirectWrite 資源の無効化キー。設定の所有者は EditorState（ADR 0020）。
    core::FontSize formatted_size_ = core::default_font_size();
    core::DisplayText formatted_family_ = core::default_editor_settings().font_family;
};
} // namespace nenenib::ui::win32
