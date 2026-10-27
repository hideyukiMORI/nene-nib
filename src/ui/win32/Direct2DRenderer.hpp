#pragma once

#include "BodyLayout.hpp"
#include "BodyPane.hpp"
#include "BodyTextLayout.hpp"
#include "BodyTextLayouts.hpp"
#include "BodyTextPainter.hpp"
#include "ClauseEmphasis.hpp"
#include "ColoredRun.hpp"
#include "CommandChoice.hpp"
#include "CommandLayout.hpp"
#include "DeviceWarmup.hpp"
#include "DisplayLine.hpp"
#include "EditorFrame.hpp"
#include "GridRowPainter.hpp"
#include "GridRowView.hpp"
#include "GutterTextLayouts.hpp"
#include "KeycapStyle.hpp"
#include "LayoutRect.hpp"
#include "LineUtf16Evaluation.hpp"
#include "LineView.hpp"
#include "OperationGuideLayout.hpp"
#include "PaletteLayout.hpp"
#include "PaneLayouts.hpp"
#include "PreviewPane.hpp"
#include "RenderFailure.hpp"
#include "RendererSources.hpp"
#include "RgbColor.hpp"
#include "StatusBarLayout.hpp"
#include "StatusLayoutDemand.hpp"
#include "StatusTextLayout.hpp"
#include "TimingPort.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
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
    // device は worker が先に作った箱から受け取り、空なら自分で作る（ADR 0131 の決定 3）。
    [[nodiscard]] static std::expected<Direct2DRenderer, RenderFailure>
    create(HWND window, std::uint32_t dpi, application::TimingPort &timing,
           RendererSources sources);
    // 起動の最初の frame のための仮の renderer（ADR 0132 の決定 2）。箱の WARP の device
    // （take_provisional）で、本窓を覆う子窓の HWND に結ぶ。本窓の HWND を渡さない。
    // 箱に WARP が無ければ device_creation で、呼び手は本窓に本物で作る。
    [[nodiscard]] static std::expected<Direct2DRenderer, RenderFailure>
    create_provisional(HWND cover, std::uint32_t dpi, application::TimingPort &timing,
                       RendererSources sources);
    // 描く前に latency の待ちを済ませ、EndDraw が返った直後に frame_drawn を打ち（#844）、
    // Present(0, 0) で渡す（ADR 0007 の追補・#888）。
    [[nodiscard]] std::expected<void, RenderFailure> render(const application::EditorFrame &frame,
                                                            application::TimingPort &timing);
    [[nodiscard]] std::expected<void, RenderFailure> resize(UINT width, UINT height);
    [[nodiscard]] std::expected<void, RenderFailure> set_dpi(std::uint32_t dpi);
    [[nodiscard]] std::expected<void, RenderFailure> set_font(const core::EditorSettings &settings);
    // 最後に描いたキャレットの物理画素。窓が IME の候補窓をその直下に置く（ADR 0014 の決定 6）。
    // 変換中は変換中のキャレット（GCS_CURSORPOS の位置）になる。
    [[nodiscard]] RECT caret_rectangle() const noexcept;
    // create_provisional で作った（WARP で子窓に描いている）とき真。
    [[nodiscard]] bool provisional() const noexcept;
    // 最後の Commit が DWM に取り込まれるまで待つ。差し替えで子窓を壊す前の 1 か所だけが呼ぶ
    // （ADR 0132 の決定 3）。
    [[nodiscard]] HRESULT wait_for_commit() const;

  private:
    // 待機可能オブジェクトは HANDLE なので所有を型で閉じる（CPP-016）。
    using WaitableHandle = std::unique_ptr<void, decltype(&::CloseHandle)>;
    using TextFormat = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
    using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

    explicit Direct2DRenderer(BodyTextLayouts &text_layouts);
    [[nodiscard]] static std::expected<Direct2DRenderer, RenderFailure>
    started(Direct2DRenderer renderer, HWND window, application::TimingPort &timing,
            DeviceWarmup &warmup);
    [[nodiscard]] std::expected<void, RenderFailure>
    initialize(HWND window, application::TimingPort &timing, DeviceWarmup &warmup);
    // 箱から受け取った factory を返す（自分で作ったときは空・swap chain が作る）。
    [[nodiscard]] std::expected<Microsoft::WRL::ComPtr<IDXGIFactory2>, RenderFailure>
    create_device(DeviceWarmup &warmup);
    [[nodiscard]] std::expected<void, RenderFailure>
    create_swap_chain(HWND window, Microsoft::WRL::ComPtr<IDXGIFactory2> factory);
    [[nodiscard]] std::expected<void, RenderFailure> bind_composition(HWND window);
    [[nodiscard]] std::expected<void, RenderFailure> create_context();
    [[nodiscard]] std::expected<void, RenderFailure> acquire_surface();
    [[nodiscard]] std::expected<void, RenderFailure> create_text_formats();
    [[nodiscard]] std::expected<void, RenderFailure> create_guide_formats(const wchar_t *face);
    [[nodiscard]] std::expected<void, RenderFailure>
    create_body_formats(const core::EditorSettings &settings);
    void align_text_formats();
    // 書式（か layout）の領域に入りきらない文字列を文字単位で切って末尾に「…」を付ける。
    // タブの題名（docs/design/2026-09-29-tabs.md 第 2 節）と一覧の場所（ADR 0057 の決定 7）が
    // 同じ設定を使う。
    [[nodiscard]] HRESULT trim_by_character(IDWriteTextFormat *format);
    [[nodiscard]] const wchar_t *family(const wchar_t *preferred, const wchar_t *fallback) const;
    [[nodiscard]] HRESULT make_format(const wchar_t *face, float size_dips,
                                      DWRITE_FONT_WEIGHT weight, TextFormat &format);
    [[nodiscard]] float scaled(float dips) const noexcept;
    void fill(const core::LayoutRect &area, core::RgbColor color);
    void fill_rounded(const core::LayoutRect &area, core::RgbColor color, float radius);
    void write(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area,
               core::RgbColor color);
    void write_status(std::string_view text, IDWriteTextFormat *format,
                      const core::LayoutRect &area, core::RgbColor color);
    [[nodiscard]] const StatusTextLayout &status_text_layout(std::string_view text,
                                                             IDWriteTextFormat *format,
                                                             const core::LayoutRect &area);
    void draw_status_text(const StatusTextLayout &entry, const core::LayoutRect &area,
                          core::RgbColor color);
    void write_gutter(std::string_view text, const core::LayoutRect &area, core::RgbColor color);
    // 欄の右端に寄せて書き、欄の外は切る。幅 0 の欄には何も書かない。
    void write_right(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area,
                     core::RgbColor color);
    void draw_cross(const core::LayoutRect &box, float half, float stroke);
    void draw_title_bar(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_tab_face(const core::TitleBarLayout &layout, const core::LayoutRect &tab,
                       core::RgbColor color);
    void draw_button_face(const application::EditorFrame &frame, const core::TitleBarLayout &layout,
                          const core::LayoutRect &box, core::TitleBarTarget target);
    void draw_tab(const application::EditorFrame &frame, const core::TitleBarLayout &layout,
                  std::size_t index);
    void draw_tabs(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_add_tab(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_tab_list(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_caption_glyphs(const core::TitleBarLayout &layout, core::RgbColor color);
    // 本文の layout を描く経路（保持した字形・範囲の描き直し・置き換え）は BodyTextPainter の
    // 1 本（ADR 0073・#531）。本文を描いている間の縦の範囲を渡した 1 回分を作る。
    [[nodiscard]] BodyTextPainter<ID2D1RenderTarget> body_painter();
    void draw_body_text(IDWriteTextLayout *text, const core::LayoutRect &area,
                        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_CLIP);
    [[nodiscard]] TextLayout layout_of(std::string_view text, const core::BodyLayout &body,
                                       const core::EditorSettings &settings,
                                       const application::LineWindow &window);
    [[nodiscard]] TextLayout text_layout(std::string_view text, IDWriteTextFormat *format,
                                         const core::LayoutRect &area);
    // 折り返し・双方向の全矩形。固定数の上限で選択やIMEの下線を切らない。
    [[nodiscard]] std::vector<DWRITE_HIT_TEST_METRICS>
    runs_of(IDWriteTextLayout *text, const core::LayoutRect &area, DWRITE_TEXT_RANGE range);
    void fill_runs(IDWriteTextLayout *text, const core::LayoutRect &area, DWRITE_TEXT_RANGE range);
    // 範囲の下端に太さ thickness の帯を引く（IME の文節の下線・ADR 0014 の決定 7）。
    void underline_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                        DWRITE_TEXT_RANGE range, std::int32_t thickness);
    // 範囲の字だけを別の色で描き直す。切り抜きの中に行の layout をもう一度通す。
    void tint_runs(IDWriteTextLayout *text, const core::LayoutRect &area, DWRITE_TEXT_RANGE range,
                   core::RgbColor color);
    // 範囲の当たり矩形を内側 1 DIP の枠で囲む（検索の現在の当たり・ADR 0037 の決定 5）。
    void outline_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
                      DWRITE_TEXT_RANGE range, float stroke);
    // 行の中の範囲。桁は application の LineSpanEvaluation が作ってある（ADR 0095）。
    [[nodiscard]] static DWRITE_TEXT_RANGE range_of(const application::LineView &line,
                                                    const core::SelectionSpan &span,
                                                    LineUtf16Evaluation &positions);
    // 置換した文字の UTF-16 範囲。IME の差し込み位置以降は inserted.length だけずらす。
    [[nodiscard]] static std::vector<DWRITE_TEXT_RANGE>
    replaced_ranges(const core::DisplayLine &line, DWRITE_TEXT_RANGE inserted);
    // 検索の当たりの面。選択より先に塗るので、重なる所は選択が勝つ（ADR 0037 の決定 5）。
    void draw_line_matches(const application::EditorFrame &frame, IDWriteTextLayout *text,
                           const core::LayoutRect &area, const application::LineView &line);
    void draw_current_match(const application::EditorFrame &frame, IDWriteTextLayout *text,
                            const core::LayoutRect &area, const application::LineView &line);
    void draw_line_selection(const application::EditorFrame &frame, IDWriteTextLayout *text,
                             const core::LayoutRect &area, const application::LineView &line);
    // 色の区間を描く layout の UTF-16 の範囲へ写す（ADR 0156 の決定 11・IME の差し込み行は #839）。
    [[nodiscard]] static std::vector<ColoredRun> colored_runs(const application::EditorFrame &frame,
                                                              const application::LineView &line);
    // 本文の字（ADR 0156 の決定 11）。色の区間が無ければ text の色で 1 回、あれば区間ごとの
    // 構文の色（core::syntax_color）と区間の外の text の色で描く。brush は 1 本のまま色を替える。
    // IME の差し込み行も同じ道（差し込んだ文字列は区間の外なので text の色・#839）。
    void draw_line_text(const application::EditorFrame &frame, IDWriteTextLayout *text,
                        const core::LayoutRect &area, const application::LineView &line);
    // 縦棒の矩形（area の中の UTF-16 の position・幅は caret_width_）。位置を引けなければ値なし。
    [[nodiscard]] std::optional<core::LayoutRect> bar_of(IDWriteTextLayout *text,
                                                         const core::LayoutRect &area,
                                                         UINT32 position,
                                                         core::CaretAffinity affinity);
    void draw_bar_caret(const application::EditorFrame &frame, IDWriteTextLayout *text,
                        const core::LayoutRect &area, UINT32 position);
    // ブロックが覆う run（本文の桁の描画上の範囲・`^M` なら 2 文字・全角なら字幅・#151）。操作中の
    // 枠のブロックともう 1 つの枠の中空のブロックが同じ幅を使う。位置を引けなければ値なし。
    [[nodiscard]] std::optional<std::vector<DWRITE_HIT_TEST_METRICS>>
    block_runs(IDWriteTextLayout *text, const core::LayoutRect &area,
               const application::LineView &shown, const application::CaretView &caret);
    void draw_block_caret(const application::EditorFrame &frame, IDWriteTextLayout *text,
                          const core::LayoutRect &area, const application::LineView &shown);
    void draw_caret(const application::EditorFrame &frame, IDWriteTextLayout *text,
                    const core::LayoutRect &area, const application::LineView &shown);
    void draw_passive_caret(IDWriteTextLayout *text, const core::LayoutRect &area,
                            const application::LineView &shown,
                            const application::CaretView &caret);
    // ブロックの run を 1 DIP の枠線で囲む（中は塗らない）。色は呼び手が置く。
    void outline_block(const std::optional<std::vector<DWRITE_HIT_TEST_METRICS>> &runs);
    // 置き換えた文字（`^M`・`<200b>`）は muted の字色で描き直す。面は塗らない（ADR 0040 の決定
    // 4）。
    // ranges は描いた layout の UTF-16 の範囲（IME の変換中の行ではずらした後・#152）。
    void draw_replaced(const application::EditorFrame &frame, IDWriteTextLayout *text,
                       const core::LayoutRect &area, std::span<const DWRITE_TEXT_RANGE> ranges);
    // 注目文節は accent の 2 DIP の下線と selection と同じ面、他の文節は ime の 1 DIP の
    // 下線と ime の字色（ADR 0014 の決定 7・採用案 D15）。
    void draw_target_clause(const application::EditorFrame &frame, IDWriteTextLayout *text,
                            const core::LayoutRect &area, DWRITE_TEXT_RANGE range);
    void draw_other_clause(const application::EditorFrame &frame, IDWriteTextLayout *text,
                           const core::LayoutRect &area, DWRITE_TEXT_RANGE range);
    // base は変換中の文字列を差し込んだ UTF-16 の位置（描画用の行の中・ADR 0040 の決定 3）。
    void draw_clauses(const application::EditorFrame &frame, IDWriteTextLayout *text,
                      const core::LayoutRect &area, UINT32 base);
    // 変換中の文字列をキャレットの位置に差し込んだ 1 行。TextBuffer は触らない（ARC-004）。
    void draw_composed_line(const application::EditorFrame &frame, const core::BodyLayout &body,
                            const core::LayoutRect &area, const application::LineView &line);
    void draw_plain_line(const application::EditorFrame &frame, const BodyPane &pane,
                         const core::LayoutRect &area, const application::LineView &line);
    void draw_line(const application::EditorFrame &frame, const BodyPane &pane,
                   const application::LineView &line);
    // 操作中の枠（body は pane_layouts の active_side の側）。
    void draw_body(const application::EditorFrame &frame, const core::BodyLayout &body);
    void draw_other_body(const application::EditorFrame &frame, const core::PaneLayouts &panes);
    // draw_other_body が std::visit で選ぶ 1 対（ADR 0155 の決定 11）。プレビューの枠は
    // preview_ が描く。
    void draw_other(const application::EditorFrame &frame, const core::BodyLayout &body,
                    const application::PaneFrame &other);
    void draw_other(const application::EditorFrame &frame, const core::BodyLayout &body,
                    const application::PreviewFrame &other);
    void draw_pane(const application::EditorFrame &frame, const BodyPane &pane);
    // 格子の画面行（ADR 0140 の決定 5）。本文の行（draw_line）とは別の 1 本で、枠の grid を描く。
    using GridPainter = GridRowPainter<ID2D1DeviceContext6, BodyTextLayouts>;
    void draw_grid_rows(const application::EditorFrame &frame, const BodyPane &pane);
    void draw_grid_row(GridPainter &painter, const application::EditorFrame &frame,
                       const BodyPane &pane, const application::GridRowView &row);
    [[nodiscard]] std::int32_t gutter_padding(const core::EditorSettings &settings) const;
    void draw_body_guide(const application::EditorFrame &frame,
                         const core::OperationGuideLayout &guide);
    void draw_status_guide(const application::EditorFrame &frame,
                           const core::OperationGuideLayout &guide);
    void draw_status_bar(const application::EditorFrame &frame,
                         const core::StatusBarLayout &layout);
    void draw_toggle(const application::EditorFrame &frame, const core::StatusBarLayout &layout);
    void draw_status_message(const application::EditorFrame &frame, const core::LayoutRect &area);
    void draw_recording(const application::EditorFrame &frame, const core::StatusBarLayout &layout,
                        const StatusTextLayout &mode);
    void draw_status_left(const application::EditorFrame &frame,
                          const core::StatusBarLayout &layout);
    void draw_command(const application::EditorFrame &frame, const core::LayoutRect &area);
    // 入力行の変換の面と下線を文字の 1 行の上下に引き、画素に揃えた上下を返す。
    [[nodiscard]] D2D1_RECT_F draw_command_clauses(const application::EditorFrame &frame,
                                                   IDWriteTextLayout *text, D2D1_RECT_F line,
                                                   UINT32 base);
    void draw_completions(const application::EditorFrame &frame,
                          const core::StatusBarLayout &status);
    void draw_completion(const application::EditorFrame &frame, const core::LayoutRect &row,
                         std::string_view candidate, bool active);
    void draw_palette(const application::EditorFrame &frame, const core::PaletteLayout &layout);
    void draw_palette_choices(const application::EditorFrame &frame,
                              const core::PaletteLayout &layout);
    void draw_palette_choice(const application::EditorFrame &frame, const core::LayoutRect &row,
                             std::size_t index);
    void draw_palette_label(const application::EditorFrame &frame,
                            const core::CommandChoice &choice, const core::LayoutRect &label);
    void draw_palette_detail(const application::EditorFrame &frame,
                             const core::CommandChoice &choice, const core::LayoutRect &label,
                             IDWriteTextLayout *title);
    // 行の右端の欄に操作の鍵を枠つきで描く（ADR 0078 の決定 12）。保持しない。
    void draw_palette_key(const application::EditorFrame &frame, std::string_view key,
                          const core::LayoutRect &note);
    void draw_keycap(std::string_view key, IDWriteTextFormat *format, const core::LayoutRect &area,
                     const KeycapStyle &style);
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
    // buffer 0 の寿命の間だけ保持し、ResizeBuffers の前に解放する（ADR 0083）。
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> surface_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<IDWriteFactory7> dwrite_;
    TextFormat tab_format_;
    TextFormat toggle_format_;
    TextFormat status_format_;
    TextFormat mode_format_;
    TextFormat command_format_;
    TextFormat gutter_format_;
    // 面の行の右端の鍵。本文の字体・UI の大きさ・左寄せ（幅を測るので右寄せにしない）。
    TextFormat key_format_;
    TextFormat guide_key_format_;
    TextFormat guide_label_format_;
    TextFormat guide_note_format_;
    TextFormat guide_status_format_;
    // 現在と直前の描画の本文資源だけ。前の列の残りは描画終了時に捨てる（ADR 0069）。
    BodyTextLayouts &body_text_;
    std::optional<RenderFailure> text_failure_;
    GutterTextLayouts gutter_layouts_;
    // other の枠のプレビュー。要素の layout の保持は frame をまたいで再利用する（ADR 0155）。
    PreviewPane preview_;
    // 通常7枠と案内4枠。録画時は案内が無く8枠（ADR 0079・言語の項目で 11・ADR 0154 の決定 6）。
    // 長さは StatusLayoutDemand.hpp の 1 つで、frame ごとの数は status_layout_demand が数える。
    std::array<StatusTextLayout, status_layout_capacity> status_layouts_{};
    std::size_t status_layout_cursor_ = 0;
    WaitableHandle latency_{nullptr, &::CloseHandle};
    RECT caret_rectangle_{};
    std::int32_t caret_width_ = 2;
    std::int32_t body_line_height_ = 1;
    // 格子の 1 升の幅（物理画素）。draw_grid_rows がその frame の書式で測る。
    float grid_cell_ = 1;
    // 本文を描いている間だけの、描く範囲の縦（本文の上端・下端）。外では限りなし（#528）。
    std::int32_t body_top_ = std::numeric_limits<std::int32_t>::min();
    std::int32_t body_bottom_ = std::numeric_limits<std::int32_t>::max();
    std::uint32_t dpi_ = 96;
    bool provisional_ = false;
    // DirectWrite 資源の無効化キー。設定の所有者は EditorState（ADR 0020）。
    core::FontSize formatted_size_ = core::default_font_size();
    core::DisplayText formatted_family_ = core::default_editor_settings().font_family;
};
} // namespace nenenib::ui::win32
