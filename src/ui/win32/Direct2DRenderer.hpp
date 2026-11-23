#pragma once

#include "BodyLayout.hpp"
#include "BodyPane.hpp"
#include "BodyTextLayout.hpp"
#include "BodyTextLayouts.hpp"
#include "BodyTextPainter.hpp"
#include "CaretMotionFollower.hpp"
#include "CaretSparksFollower.hpp"
#include "ClauseEmphasis.hpp"
#include "ColoredRun.hpp"
#include "CommandChoice.hpp"
#include "CommandLayout.hpp"
#include "CompletionFaceLayout.hpp"
#include "CompletionPainter.hpp"
#include "ContextMenuLayout.hpp"
#include "ContextMenuPainter.hpp"
#include "DeviceWarmup.hpp"
#include "DisplayLanguage.hpp"
#include "DisplayLine.hpp"
#include "EditorFrame.hpp"
#include "ExplorerLayout.hpp"
#include "ExplorerPainter.hpp"
#include "GridRowPainter.hpp"
#include "GridRowView.hpp"
#include "GuideRequest.hpp"
#include "GutterTextLayouts.hpp"
#include "HoverTipLayout.hpp"
#include "IndentGuidePaintMemo.hpp"
#include "KeycapStyle.hpp"
#include "LayoutRect.hpp"
#include "LineUtf16Evaluation.hpp"
#include "LineView.hpp"
#include "LinkCardFormats.hpp"
#include "LinkCardPaint.hpp"
#include "LinkCardTarget.hpp"
#include "MinimapPicture.hpp"
#include "MotionLabel.hpp"
#include "OperationGuideLayout.hpp"
#include "PaletteLayout.hpp"
#include "PaletteRowLayouts.hpp"
#include "PaletteTextRole.hpp"
#include "PaneLayouts.hpp"
#include "PreviewPane.hpp"
#include "PreviewPanePainter.hpp"
#include "PreviewPaneText.hpp"
#include "PreviewPoint.hpp"
#include "PreviewScrollSettled.hpp"
#include "PreviewTableHit.hpp"
#include "ProjectHitRow.hpp"
#include "ProjectHitText.hpp"
#include "RenderFailure.hpp"
#include "RendererSources.hpp"
#include "RgbColor.hpp"
#include "SettingsControl.hpp"
#include "SettingsLayout.hpp"
#include "SettingsPanelFormats.hpp"
#include "SettingsPanelPainter.hpp"
#include "SettingsSection.hpp"
#include "SettingsTarget.hpp"
#include "SettingsView.hpp"
#include "StartScreenPainter.hpp"
#include "StatusBarLayout.hpp"
#include "StatusLayoutDemand.hpp"
#include "StatusTextLayout.hpp"
#include "TabTipLayout.hpp"
#include "TimingPort.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"
#include "WaveBand.hpp"

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
#include <string>
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
    // 最後に描いたプレビューの枠の点（クライアントの物理画素）の下のチェックボックスの要素の番号
    // （ADR 0155 の追補 24）。窓が押した・離した点を引く。
    [[nodiscard]] std::optional<std::size_t> preview_task_at(std::int32_t x, std::int32_t y) const;
    // 最後に描いた右クリックのメニューの点（クライアントの物理画素）の下の行と、面の中か（ADR 0078
    // の追補 #1051）。描いた矩形をそのまま引く（描画と当たりが同じ core::context_menu_layout）。
    // メニューを描いていなければ無し・偽。
    [[nodiscard]] std::optional<std::size_t> context_menu_row_at(std::int32_t x,
                                                                 std::int32_t y) const;
    [[nodiscard]] bool context_menu_covers(std::int32_t x, std::int32_t y) const;
    // 最後に描いた補完の面の点（クライアントの物理画素）の下の候補（今の接頭辞に当たる候補の中の
    // 位置）と、面の中か（ADR 0190 の決定 5・#1844 の R1）。面を描いていなければ無し・偽。
    [[nodiscard]] std::optional<std::size_t> completion_row_at(std::int32_t x,
                                                               std::int32_t y) const;
    [[nodiscard]] bool completion_covers(std::int32_t x, std::int32_t y) const;
    // 最後に描いた設定の面（施主決定 D102・#1836）。描いた配置（core::settings_layout の 1 本）を
    // そのまま引く。面を描いていなければ outside・値なし。
    // - settings_target_at: 点（クライアントの物理画素）の下の部品。
    // - settings_layout: 最後に描いた配置（PageUp / PageDown の行き先を引く）。
    // - settings_scrolled: 送り from からホイールで lines 行送った送り（変わらなければ値なし）。
    //   from は application の状態の送り（描画の前に 2 通来ても 1 通目が消えない）。
    // - settings_revealed: view の焦点の行（section があればその節の見出しから終わりまで）が見える
    //   送り（今の送りのままでよければ値なし）。配置は最後に描いた帯で作り直す（描く前の view でも
    //   引ける）。
    // - settings_search_rectangle: 探す欄（IME の変換の窓を置く所）。
    [[nodiscard]] core::SettingsTarget settings_target_at(std::int32_t x, std::int32_t y) const;
    [[nodiscard]] const std::optional<core::SettingsLayout> &settings_layout() const noexcept;
    [[nodiscard]] std::optional<std::int32_t> settings_scrolled(std::int32_t from,
                                                                std::int32_t lines) const;
    [[nodiscard]] std::optional<std::int32_t>
    settings_revealed(const application::SettingsView &view, core::DisplayLanguage language,
                      std::optional<core::SettingsSection> section);
    [[nodiscard]] std::optional<core::LayoutRect> settings_search_rectangle() const;
    // 最後に描いたプレビューの枠の点（クライアントの物理画素）の下の文字の位置（ADR 0155 の改訂
    // 18）。全面プレビューのドラッグの選択が引く。
    [[nodiscard]] std::optional<core::PreviewPoint> preview_point_at(std::int32_t x,
                                                                     std::int32_t y);
    // 最後に描いたプレビューの枠の点の真下の文字（文字の上のときだけ・リンクの札・#1869 の段 L2）。
    [[nodiscard]] std::optional<core::PreviewPoint> preview_character_at(std::int32_t x,
                                                                         std::int32_t y);
    // 最後に描いたプレビューの枠の点の下の枠より広い表と、つかんだ表の棒の溝の上の横の位置
    // （#1883・PreviewPane::table_at / table_bar）。
    [[nodiscard]] std::optional<PreviewTableHit> preview_table_at(std::int32_t x, std::int32_t y);
    [[nodiscard]] std::optional<PreviewTableHit> preview_table_bar(std::size_t table,
                                                                   std::int32_t x);
    // 最後の frame でプレビューの縦の送りを畳んで位置が変わったときの答え（#1894・
    // PreviewPane::settled）。ui は描いた後に読んで 1 回だけ送る。
    [[nodiscard]] const std::optional<application::PreviewScrollSettled> &
    preview_settled() const noexcept;
    // キャレットの動き（ADR 0192 の決定 3・6）。allowed は Windows の「アニメーション効果」、
    // floor は窓が最後に追った動きの番号で、窓が描く前に毎回置く（作り直した renderer でも番号を
    // 下げない・#1956 の R2）。generation は最後の frame でキャレットがまだ動いていれば今の動きの
    // 番号、止まっていれば値なし（窓は描いた後に読み、動いている間だけ合図を送る）。settle は
    // 覚えを捨てる（窓が見えなくなったとき・次の frame は跳ぶ）。
    void allow_caret_motion(bool allowed, std::uint32_t floor) noexcept;
    [[nodiscard]] std::optional<std::uint32_t> caret_motion_generation() const noexcept;
    void settle_caret_motion() noexcept;
    // 打ったときの粒（ADR 0192 の追補・#1979）。動きと同じ形で、allowed と floor は窓が描く前に毎回
    // 置き、generation は粒が生きていれば粒の時計の番号（窓は描いた後に読み、生きている間だけ合図を
    // 送る）、settle は生きている粒を捨てる。
    void allow_caret_sparks(bool allowed, std::uint32_t floor) noexcept;
    [[nodiscard]] std::optional<std::uint32_t> caret_sparks_generation() const noexcept;
    void settle_caret_sparks() noexcept;
    // 最後に描いたリンクの札の点の下の部品（札を描いていなければ outside・#1869 の段 L2）。
    [[nodiscard]] LinkCardTarget link_card_target_at(std::int32_t x, std::int32_t y) const;
    // 最後に描いた hover の札が点（クライアントの物理画素）を覆うか（描いていなければ偽・#1884 の
    // レビュー R1）。
    [[nodiscard]] bool hover_tip_covers(std::int32_t x, std::int32_t y) const;

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
    // 表示言語が書式を作ったときと違えば、画面の文字の書式を作り直す（UI 書式の再作成の道）。
    [[nodiscard]] std::expected<void, RenderFailure> set_language(core::DisplayLanguage language);
    [[nodiscard]] std::expected<void, RenderFailure> create_guide_formats(const wchar_t *face);
    [[nodiscard]] bool create_explorer_formats(const wchar_t *face);
    // タブの札の文（#1789）。UI の字体・12 DIP・折り返さない・左寄せ。作れなければ偽。
    [[nodiscard]] bool create_tab_tip_format(const wchar_t *face);
    // 全文検索の当たりの行の 2 段と入力欄の右の進み具合（ADR 0167 の決定
    // 5・採用案の寸法）。上の段は 本文の字体 body_face、下の段と進み具合は UI
    // の字体。作れなければ偽。
    [[nodiscard]] bool create_project_hit_formats(const wchar_t *body_face);
    // プレビュー欄の本文・行番号（本文の字体 body_face・12.5 DIP）と見出し（UI の字体・11.5 DIP・
    // ADR 0189 の決定 5）。作れなければ偽。
    [[nodiscard]] bool create_preview_pane_formats(const wchar_t *body_face);
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
    // 斜体の書式（プレビューのタブの題名・ADR 0179 の決定 10）。ほかは make_format と同じ。
    [[nodiscard]] HRESULT make_italic_format(const wchar_t *face, float size_dips,
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
    void write_gutter(std::string_view text, core::GutterAlignment alignment,
                      const core::LayoutRect &area, core::RgbColor color);
    // 欄の右端に寄せて書き、欄の外は切る。幅 0 の欄には何も書かない。
    void write_right(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area,
                     core::RgbColor color);
    void draw_cross(const core::LayoutRect &box, float half, float stroke);
    void draw_title_bar(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_tab_face(const core::TitleBarLayout &layout, const core::LayoutRect &tab,
                       core::RgbColor color);
    void draw_button_face(const application::EditorFrame &frame, const core::TitleBarLayout &layout,
                          const core::LayoutRect &box, core::TitleBarTarget target);
    void write_tab_title(std::string_view text, IDWriteTextFormat *format,
                         const core::LayoutRect &title, core::RgbColor color);
    void draw_tab(const application::EditorFrame &frame, const core::TitleBarLayout &layout,
                  std::size_t index);
    void draw_tabs(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_add_tab(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    void draw_tab_list(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    // タブの札（#1789）。frame に札のタブがあるときだけ描く。いちばん最後に描く（本文・面の上）。
    void draw_tab_tip(const application::EditorFrame &frame, const core::TitleBarLayout &layout);
    // hover の札（ADR 0190 の決定 6）。この frame
    // で描いたキャレットの矩形の上（入らなければ下）に、 本文の領域 area
    // の中で描く。文が無ければ何もしない。
    void draw_hover_tip(const application::EditorFrame &frame, const core::LayoutRect &area);
    // 引数の情報の札（#1959・ADR 0191 の決定 6）。hover の札と同じ部品で、今の引数を太字にする。
    void draw_signature_tip(const application::EditorFrame &frame, const core::LayoutRect &area,
                            const core::LayoutRect &pane);
    // リンクの札（施主決定 D106・#1869 の段 L2）。この frame で描いたプレビューのリンクの下（入らな
    // ければ上）に、本文の領域 area の中で描く。札が無ければ配置の覚えを捨てる。
    void draw_link_card(const application::EditorFrame &frame, const core::LayoutRect &area);
    [[nodiscard]] bool link_card_formats_made();
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
    // イージーモーションの印（ADR 0163 の決定 6 と追補 #1800・案 D）。跳び先の升の左上の肩に text
    // の札を置き、印の字を background で札の中央に描く。行の札は左から右の順（重なれば右が上）。
    // 印の字の layout は印の専用の書式（label_format_）から作り、共有の本文の layout の書式は
    // 変えない（ADR 0114）。いちばん上の画面行の札は本文の欄の上端（body_top_）より上へ出さない。
    void draw_line_labels(const application::EditorFrame &frame, IDWriteTextLayout *text,
                          const core::LayoutRect &area, const application::LineView &line);
    void draw_label(const application::EditorFrame &frame, const DWRITE_HIT_TEST_METRICS &target,
                    const core::MotionLabel &label);
    // 印の字の書式（本文の字体の太字・motion_label_text_dips・中央寄せ）。印が出たときに初めて
    // 作り、本文の書式を作り直す所（DPI・字体・大きさの変更）で捨てる。作れなければ偽。
    [[nodiscard]] bool ensure_label_format();
    // 全部の行を描いた後の印の 1 パスと、その後のキャレット。印の無い frame は入口で抜ける。
    void draw_label_pass(const application::EditorFrame &frame, const BodyPane &pane);
    void draw_pass_caret(const application::EditorFrame &frame, const BodyPane &pane);
    // 本文の桁の区間（begin と end を持つ Span の列）を描く layout の UTF-16 の範囲へ写す 1 本
    // （色の区間と診断の下線の共用・IME の差し込み行は #839）。返す列は spans と同じ並びと長さ。
    template <typename Span>
    [[nodiscard]] static std::vector<DWRITE_TEXT_RANGE>
    span_ranges(const application::LineView &line, const std::vector<Span> &spans);
    // 色の区間を描く layout の UTF-16 の範囲へ写す（ADR 0156 の決定 11・IME の差し込み行は #839）。
    [[nodiscard]] static std::vector<ColoredRun> colored_runs(const application::EditorFrame &frame,
                                                              const application::LineView &line);
    // 本文の字（ADR 0156 の決定 11）。色の区間が無ければ text の色で 1 回、あれば区間ごとの
    // 構文の色（core::syntax_color）と区間の外の text の色で描く。brush は 1 本のまま色を替える。
    // IME の差し込み行も同じ道（差し込んだ文字列は区間の外なので text の色・#839）。
    void draw_line_text(const application::EditorFrame &frame, IDWriteTextLayout *text,
                        const core::LayoutRect &area, const application::LineView &line);
    // 診断の波線（ADR 0161 の決定 4）。区間の run の下端に draw_diagnostic_wave で引く。行末の外の
    // 区間（空の診断）は行末から block_minimum の幅。区間が無い行は分岐 1 つで抜ける。
    void draw_line_diagnostics(const application::EditorFrame &frame, IDWriteTextLayout *text,
                               const core::LayoutRect &area, const application::LineView &line);
    void draw_line_end_wave(IDWriteTextLayout *text, const core::LayoutRect &area, UINT32 position);
    // 診断の波線の帯（物理画素・DPI で写した振幅・太さ・周期）。bottom は run の下端。
    [[nodiscard]] WaveBand wave_band(float left, float right, float bottom) const;
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
    // 本文の「下の飾り」（ADR 0193 の決定 2）。選択の面の後・本文の字の前に、インデントガイド →
    // （段 3 の）ペアガイドの順で描く。順を知るのはこの 1 本で、段 2・3 の飾りもここに足す。
    void draw_decorations_under(const BodyPane &pane, const core::LayoutRect &area,
                                const application::LineView &line);
    void draw_indent_guides(const BodyPane &pane, const core::LayoutRect &area,
                            const application::LineView &line);
    // frame の初めに飾りの色の材料を覚えから引く（オフなら真偽 1 つ・ADR 0193 の決定 6）。
    void begin_decorations(const application::EditorFrame &frame);
    void draw_line(const application::EditorFrame &frame, const BodyPane &pane,
                   const application::LineView &line);
    // 本文の面の全部（area は面を除いた client・全面のプレビューか枠と案内・ADR 0155 の改訂 15）。
    void draw_body_area(const application::EditorFrame &frame, const core::LayoutRect &area,
                        const core::PaneLayouts &panes, const core::OperationGuideLayout &guide);
    // 操作中の枠（body は pane_layouts の active_side の側）。
    void draw_body(const application::EditorFrame &frame, const core::BodyLayout &body);
    // キャレットの動き（ADR 0192）。begin と finish は draw の 1 本の入口と出口（本文を描かない
    // 全面のプレビューの frame も通る・#1956 の M1）で、begin は frame ごとに動かすかを決め（オフは
    // 欄 1 つを読むだけ）、finish は見えなかったキャレットの覚えを捨てる。moves は行き先の矩形で
    // 覚えを進めて「まだ動いている（ふつうのキャレットを描かない）」を返し、paint は動いていれば
    // 本文の上に多角形を重ねる（操作中の枠を描いた直後）。
    void begin_caret_motion(const application::EditorFrame &frame, const core::BodyLayout &body);
    [[nodiscard]] bool caret_moves(std::optional<D2D1_RECT_F> box);
    void paint_caret_motion(const application::EditorFrame &frame);
    // 打ったときの粒（#1979）。see はキャレットの行き先の矩形の中心を覚え（値なしは何もしない）、
    // paint はこの frame の覚えを進めて生きている粒を本文の領域に切って描く（操作中の枠の直後）。
    void see_caret_sparks(const std::optional<D2D1_RECT_F> &box) noexcept;
    void paint_caret_sparks(const application::EditorFrame &frame, const core::BodyLayout &body);
    void finish_caret_motion() noexcept;
    [[nodiscard]] bool grid_caret_moves(const GridRowCanvas &canvas,
                                        const application::GridRowView &row, bool active);
    [[nodiscard]] std::optional<D2D1_RECT_F> caret_box(const application::EditorFrame &frame,
                                                       IDWriteTextLayout *text,
                                                       const core::LayoutRect &area,
                                                       const application::LineView &shown);
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
    [[nodiscard]] std::int32_t body_guide_key_width(const application::EditorFrame &frame);
    // 案内の文脈と鍵の幅（起動画面が出ている frame は hidden・ADR 0172 の決定 1）。
    [[nodiscard]] core::GuideRequest guide_request(const application::EditorFrame &frame);
    void draw_body_guide(const application::EditorFrame &frame,
                         const core::OperationGuideLayout &guide);
    // 本文の案内の鍵の枠の寸法と色（docs/design/2026-10-06-guide.md の 2 節）。起動画面の鍵も
    // 同じ値（ADR 0172 の決定 5）。
    [[nodiscard]] static KeycapStyle guide_keycap_style(const core::Palette &palette) noexcept;
    // 起動画面（ADR 0172 の決定 5）。frame に値があれば本文の案内の代わりに操作中の枠 body に描く。
    // 値が無い frame は保持を捨てるだけ。
    void draw_start_screen(const application::EditorFrame &frame, const core::BodyLayout &body);
    // 起動画面の鍵の枠の列の幅（物理画素）。鍵 3 本の文字列が前と同じなら測り直さない。
    [[nodiscard]] std::int32_t start_key_width(const std::array<core::GuideEntry, 3> &keys);
    void draw_status_guide(const application::EditorFrame &frame,
                           const core::OperationGuideLayout &guide);
    // 描いたモードの文の幅（物理画素・モードを描かない frame は 0）を返す。案内を入るかで決めるため
    // （status_guide_fitted・#1658）。
    [[nodiscard]] std::int32_t draw_status_bar(const application::EditorFrame &frame,
                                               const core::StatusBarLayout &layout);
    void draw_toggle(const application::EditorFrame &frame, const core::StatusBarLayout &layout);
    void draw_status_message(const application::EditorFrame &frame, const core::LayoutRect &area);
    void draw_recording(const application::EditorFrame &frame, const core::StatusBarLayout &layout,
                        const StatusTextLayout &mode);
    // 左側を描き、モードを描いたときはその文の幅（物理画素）を、そうでなければ 0 を返す。録画の欄は
    // position（行と桁の文字組み）と並べて測った配置（status_bar_fitted・#1658）の中に書く。
    [[nodiscard]] std::int32_t draw_status_left(const application::EditorFrame &frame,
                                                const core::StatusBarLayout &layout,
                                                const StatusTextLayout &position);
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
    // 全文検索の当たりの行（ADR 0167 の決定 5）。2 段で描く経路は ProjectHitPainter の 1 本。
    void draw_project_hit(const application::EditorFrame &frame, const core::LayoutRect &row,
                          const core::ProjectHitRow &hit);
    // 候補が無い面の行の文（全文検索の面で探していない・探せない状態ならその 1 行）。
    [[nodiscard]] static std::string_view
    empty_palette_text(const application::CommandPaletteView &palette,
                       core::DisplayLanguage language);
    [[nodiscard]] ProjectHitText project_hit_text() const;
    [[nodiscard]] PreviewPaneText preview_pane_text() const;
    // プレビュー欄（ADR 0189 の決定 5）。描く経路は PreviewPanePainter の 1 本。
    void draw_preview_pane(const application::EditorFrame &frame,
                           const core::PaletteLayout &layout);
    // 面が開いていれば描き、閉じていれば文字組みの保持を捨てる。
    void draw_open_palette(const application::EditorFrame &frame, std::int32_t width,
                           std::int32_t height);
    // 入力行に描く文の幅（物理画素・進み具合が検索語と重ならないかを決める・V-1773 の L-1）。
    [[nodiscard]] std::int32_t typed_width(const application::EditorFrame &frame,
                                           const core::LayoutRect &area);
    // 全文検索の面の入力欄の右の進み具合（探している間と、探し終えた件数）。
    void draw_palette_progress(const application::EditorFrame &frame,
                               const core::PaletteLayout &layout);
    // 行の右端の欄の操作の鍵の文字組み（ADR 0078 の決定 12）。面の保持から引く。
    [[nodiscard]] TextLayout palette_key_text(std::string_view key, const core::LayoutRect &row);
    // 行の右端の欄（補足か鍵の枠・#1658）。鍵は枠の幅を測って core の配置に渡す。どちらも無い行は
    // 右端の幅 0。
    [[nodiscard]] core::LayoutRect palette_choice_note(const core::CommandChoice &choice,
                                                       const core::LayoutRect &row,
                                                       IDWriteTextLayout *key) const;
    void write_palette_note(std::string_view text, const core::LayoutRect &area,
                            core::RgbColor color);
    // 面の行の欄の文字組み。鍵が同じなら作り直さない（#900）。
    [[nodiscard]] TextLayout palette_text(std::string_view text, IDWriteTextFormat *format,
                                          PaletteTextRole role, const core::LayoutRect &area);
    [[nodiscard]] TextLayout made_palette_text(std::string_view text, IDWriteTextFormat *format,
                                               PaletteTextRole role, const core::LayoutRect &area);
    void draw_keycap(std::string_view key, IDWriteTextFormat *format, const core::LayoutRect &area,
                     const KeycapStyle &style);
    void draw_keycap_layout(IDWriteTextLayout *shown, const core::LayoutRect &note,
                            const KeycapStyle &style);
    void draw_palette_footer(const application::EditorFrame &frame, const core::LayoutRect &area);
    // 検索欄の記号の案内（#1640）。組の境目の幅を測り、どの組から出すかは core の配置が決める。
    void draw_palette_hint(const application::EditorFrame &frame,
                           const core::PaletteLayout &layout);
    // エクスプローラーの面（ADR 0162 の決定 8）。面が無い frame は保持を捨てるだけ。
    void draw_explorer(const application::EditorFrame &frame, const core::ExplorerLayout &layout);
    // ミニマップ（ADR 0171 の決定 1・2）。面が無い frame は最初の 1 行で返る。
    void draw_minimap(const application::EditorFrame &frame, const core::LayoutRect &panel);
    // 右クリックのメニュー（#1051）。ほかの全部の上に描き、描いた配置を当たりのために覚える。
    void draw_context_menu(const application::EditorFrame &frame, std::int32_t width,
                           std::int32_t height);
    // 補完の面（ADR 0169 の決定 4・ADR 0190 の決定 5）。本文と面とステータスバーの上・タブの札と
    // 右クリックのメニューの下に描く。bounds は面を収める矩形。
    void draw_completion(const application::EditorFrame &frame, const core::LayoutRect &bounds);
    // 設定の面（#1836）。area はタイトルバーの下からステータスバーの上まで。面が無い frame は最初の
    // 1 行で返る（書式も配置も作らない）。
    void draw_settings(const application::EditorFrame &frame, const core::LayoutRect &area);
    [[nodiscard]] bool settings_formats_made();
    [[nodiscard]] std::expected<void, RenderFailure> draw(const application::EditorFrame &frame,
                                                          ID2D1Bitmap1 *surface);
    // ミニマップの命令の失敗で落ちた frame を描き直す（1 度目は bitmap を捨てて直接、それでも
    // ミニマップの失敗なら面を出さずに）。
    [[nodiscard]] std::expected<void, RenderFailure>
    redraw_without_minimap_bitmap(const application::EditorFrame &frame);

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
    // プレビューのタブの題名（ADR 0179 の決定 10）。tab_format_ と同じ字体・大きさ・省略記号の
    // 斜体。色は変えない。帯の保持は書式の同一性を鍵に含むので、普通の題名と分かれる。
    TextFormat tab_transient_format_;
    TextFormat toggle_format_;
    TextFormat status_format_;
    TextFormat mode_format_;
    TextFormat command_format_;
    TextFormat gutter_format_;
    // イージーモーションの印の字（ensure_label_format・#1800）。
    TextFormat label_format_;
    // 面の行の右端の鍵。本文の字体・UI の大きさ・左寄せ（幅を測るので右寄せにしない）。
    TextFormat key_format_;
    // エクスプローラーの面の名前。UI
    // の字体・タブの題名と同じ大きさ（本文の字の大きさに追従しない）・ 文字単位の省略記号（ADR
    // 0162・#1390）。strong は semibold（ルート名とアクティブな文書）。
    TextFormat explorer_format_;
    TextFormat explorer_strong_format_;
    // 全文検索の当たりの行の上の段（本文の字体・12.5 DIP）と下の段（UI の字体・11 DIP）。どちらも
    // 文字単位の省略記号。進み具合は UI の字体・11.5 DIP の右寄せ（ADR 0167 の決定 5・採用案）。
    TextFormat hit_line_format_;
    TextFormat hit_place_format_;
    TextFormat palette_progress_format_;
    // プレビュー欄の本文と行番号（本文の字体・12.5 DIP・本文は折り返さず切る・行番号は右寄せ）と
    // 見出しの名前（UI の字体・11.5 DIP の semibold・文字単位の省略記号）とほかの文（11.5 DIP）
    // （ADR 0189 の決定 5・採用案）。
    TextFormat preview_line_format_;
    TextFormat preview_number_format_;
    TextFormat preview_name_format_;
    TextFormat preview_note_format_;
    TextFormat guide_key_format_;
    TextFormat guide_label_format_;
    TextFormat guide_note_format_;
    TextFormat guide_status_format_;
    // 空の文書の案内の鍵の箱の幅（#1122）。鍵 3 本の文字列（モードで変わる）が前と同じなら
    // 測り直さない。書式（DPI と字体）を作り直す create_guide_formats で捨てる。
    std::optional<std::array<std::string, 3>> measured_guide_keys_;
    std::int32_t measured_guide_key_width_ = 0;
    // 起動画面の題（UI の字体・本文の字の 1.5 倍）。本文の書式と一緒に作り直す（ADR 0172 の
    // 決定 5）。行の字はエクスプローラーの面の書式を共用する（追補 2026-11-11 の 21）。
    TextFormat start_title_format_;
    // 起動画面の鍵の枠の列の幅（measured_guide_keys_ と同じ形・書式の作り直しで捨てる）。
    std::optional<std::array<std::string, 3>> measured_start_keys_;
    std::int32_t measured_start_key_width_ = 0;
    // 現在と直前の描画の本文資源だけ。前の列の残りは描画終了時に捨てる（ADR 0069）。
    BodyTextLayouts &body_text_;
    std::optional<RenderFailure> text_failure_;
    GutterTextLayouts gutter_layouts_;
    // Ctrl+P の面の見えている行の文字組み。書式の作り直しと面を閉じたときに捨てる（#900）。
    PaletteRowLayouts palette_layouts_;
    // プレビュー欄の文字組み（ADR 0189 の決定 5）。面の行の保持と分け、上限は窓の行の数に合わせた
    // PreviewPanePainter::layout_limit。書式の作り直しと面を閉じたときに捨てる。
    PaletteRowLayouts preview_layouts_{PreviewPanePainter::layout_limit};
    // タブの題名の文字組み。同じ型で、書式・DPI の作り直しでだけ捨てる（#924）。
    PaletteRowLayouts tab_layouts_;
    // タブの札の文の書式と文字組みの保持（#1789・上限 1）。保持は書式の作り直しで捨てる。
    TextFormat tab_tip_format_;
    TabTipLayout tab_tip_layout_;
    HoverTipLayout hover_tip_layout_;
    // 引数の情報の札の layout（hover の札と別の 1 本・#1959）。
    HoverTipLayout signature_tip_layout_;
    // 最後に描いた hover の札の箱（描いていなければ値なし）。
    std::optional<D2D1_RECT_F> hover_tip_box_;
    // エクスプローラーの面の行の文字組み。同じ型で上限だけ大きい。書式の作り直しと面を消した
    // ときに捨てる（ADR 0162 の決定 8）。
    PaletteRowLayouts explorer_layouts_{ExplorerPainter::retained};
    // エクスプローラーの線の形と本文の角の形（V-1428 の L-1）。捨てる時は explorer_layouts_
    // と同じ。
    ExplorerShapes explorer_shapes_;
    // 起動画面の文字組み。同じ型で、書式の作り直しと起動画面が消えたときに捨てる（ADR 0172 の
    // 決定 5）。
    PaletteRowLayouts start_layouts_{StartScreenPainter::retained};
    // 右クリックのメニューの文字組みと、最後に描いた配置（#1051）。捨てる時は start_layouts_ と同じ
    // （メニューを閉じたときも）。
    PaletteRowLayouts context_menu_layouts_{ContextMenuPainter::retained};
    std::optional<core::ContextMenuLayout> context_menu_layout_;
    // 補完の面の文字組みと、面の左端の覚え（面の通し番号と、開いたときのキャレットの左・ADR 0190 の
    // 決定 5）。捨てる時は context_menu_layouts_ と同じ（面を閉じたときも）。
    PaletteRowLayouts completion_layouts_{CompletionPainter::retained};
    std::optional<std::pair<std::uint64_t, std::int32_t>> completion_anchor_;
    // 最後に描いた補完の面の配置と、その表示値の first（当たりが描画と同じ値を読む）。
    std::optional<std::pair<core::CompletionFaceLayout, std::size_t>> completion_layout_;
    // 設定の面の書式（最初に描くときに作る）・文字組み・最後に描いた配置と帯（#1836）。書式の
    // 作り直しで書式と文字組みを捨て、面を閉じたら文字組みと配置を捨てる。
    SettingsPanelFormats settings_formats_;
    // リンクの札の書式（最初に描くときに作り、書式の作り直しで捨てる）と、最後に描いた札（当たりが
    // 描画と同じ値を読む・#1869 の段 L2）。
    LinkCardFormats link_card_formats_;
    std::optional<LinkCardPaint> link_card_paint_;
    PaletteRowLayouts settings_layouts_{SettingsPanelPainter::retained};
    std::optional<core::SettingsLayout> settings_layout_;
    core::LayoutRect settings_area_{};
    // other の枠のプレビュー。要素の layout の保持は frame をまたいで再利用する（ADR 0155）。
    PreviewPane preview_;
    MinimapPicture minimap_;
    // この frame の EndDraw の失敗がミニマップの命令（minimap_tag）のものだったか。描き直しで
    // bitmap の道を捨て、2 度目は面を出さない（窓を閉じない・ADR 0171 の追補 #1449）。
    bool minimap_failed_ = false;
    bool minimap_hidden_ = false;
    // 通常7枠と案内4枠。録画時は案内が無く8枠（ADR 0079・言語の項目で 11・ADR 0154 の決定 6）。
    // 長さは StatusLayoutDemand.hpp の 1 つで、frame ごとの数は status_layout_demand が数える。
    std::array<StatusTextLayout, status_layout_capacity> status_layouts_{};
    std::size_t status_layout_cursor_ = 0;
    WaitableHandle latency_{nullptr, &::CloseHandle};
    RECT caret_rectangle_{};
    // キャレットの動きの覚え（ADR 0192 の決定 2・3）。renderer を作り直すと空から始まる（跳ぶ・
    // 番号は窓が allow_caret_motion で下げさせない）。
    CaretMotionFollower caret_motion_{};
    // 打ったときの粒の覚え（ADR 0192 の追補・#1979）。作り直すと空から始まる（番号は窓が
    // allow_caret_sparks で下げさせない）。
    CaretSparksFollower caret_sparks_{};
    // インデントガイドの色の材料の覚え（ADR 0193 の決定 5・6）。`features=indentguides` が
    // オフの frame では描かない印だけ。テーマ・単色 / 虹・色の数が変わった frame で作り直す。
    IndentGuidePaintMemo guide_paint_{};
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
    // 画面の文字の書式を作った表示言語（locale の鍵・ADR 0123 の追補 2026-11-13 の決定 9）。
    core::DisplayLanguage formatted_language_ = core::DisplayLanguage::japanese;
};
} // namespace nenenib::ui::win32
