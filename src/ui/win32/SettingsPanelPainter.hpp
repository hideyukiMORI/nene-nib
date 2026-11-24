#pragma once

#include "LanguageServerRowAction.hpp"
#include "LanguageServerRowState.hpp"
#include "LanguageServerRowView.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "RgbColor.hpp"
#include "RgbaColor.hpp"
#include "SettingsChoiceLayout.hpp"
#include "SettingsChoiceView.hpp"
#include "SettingsLayout.hpp"
#include "SettingsLayoutItem.hpp"
#include "SettingsPanelText.hpp"
#include "SettingsRowView.hpp"
#include "SettingsView.hpp"
#include "UiMessage.hpp"

#include <cstddef>
#include <cstdint>
#include <d2d1.h>
#include <dwrite.h>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
// 設定の面（右の引き出し）を描く経路（施主決定 D102・採用案
// docs/design/2026-11-18-settings-modal-look.md・#1836）。面は地 panel・左の境 1 DIP の
// panel_border（影は付けない）。見出し・鍵の表示・✕・探す欄・節の札・行の領域（clip）・下の帯の順に
// 描く。スイッチのオンは accent・オフは toggle・つまみは on_accent。選ぶ欄と段の上下は地が
// background で枠が panel_border。段の印は low が muted の ●・medium が accent の ●●・high が
// heavy（構文の色の error）の ●●● で、裏で動く機能は「裏」の語を添える。焦点の行は
// current_line の面 ＋ text の 1 DIP の枠（#1929）。選ぶ欄の一覧（#1926）は面のほかの全部の上に
// 描く（地 background・枠 panel_border・今の値に ✓・選ぼうとしている行は selection の面 ＋ text の
// 枠・値の箱の枠は text）。
// 配置は core::settings_layout の 1 本（窓の当たりと同じ矩形）で、札・組・ボタンの文字の幅は
// arranged が測って渡す。文字組みは PaletteRowLayouts の保持から引く（1 行ぶんの広い箱で作り、
// 中央と右寄せは幅を測って置く）。欄に入り切らない文は末尾を「…」で切る。target は製品では Direct2D
// の描画先、tests/ui は WIC の bitmap の 描画先。色は Palette のトークンと heavy だけ（ADR 0008）。
class SettingsPanelPainter final
{
  public:
    // 保持の上限。行ごとに名前・説明・`:set` の名前・値・段の語と、札・組・見出し・ボタン。言語
    // サーバーの節の行（表の 15 行と表に無い言語の行）は 1 行に名前・サーバー・状態・2
    // 行目・ボタン。選ぶ欄の一覧は全部の選択肢の幅を測る（配色の 126 前後・見える 10 行の文字組みは
    // 測った文字組みと同じ鍵）ので、その分を足す（#1926）。
    static constexpr std::size_t retained = 480;

    SettingsPanelPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                         const SettingsPanelText &text, PaletteRowLayouts &layouts) noexcept;
    // 文字の幅を測って配置を作る。area は面を置ける帯（タイトルバーの下からステータスバーの上）。
    [[nodiscard]] core::SettingsLayout arranged(const application::SettingsView &view,
                                                const core::LayoutRect &area) const;
    // heavy は本文の構文の色の error（段の印の「重い」と言語サーバーの節の「止めました」）。
    void paint(const application::SettingsView &view, const core::SettingsLayout &layout,
               const core::Palette &palette, core::RgbColor heavy);
    // 選ぶ欄の一覧の配置（一覧が開いていなければ値なし・#1926）。錨は焦点の行の値の箱、出してよい所は
    // 面の行の領域の幅で面の上下いっぱい、幅は選択肢の名前を測って渡す。
    // 最後の paint が描いた一覧の配置（描いていなければ値なし・窓の当たりがこれを引く）。
    [[nodiscard]] const std::optional<core::SettingsChoiceLayout> &painted_choice() const noexcept;
    [[nodiscard]] std::optional<core::SettingsChoiceLayout>
    arranged_choice(const application::SettingsView &view,
                    const core::SettingsLayout &layout) const;

  private:
    void paint_choice(const application::SettingsChoiceView &choice,
                      const core::SettingsChoiceLayout &layout);
    void paint_check(const core::LayoutRect &mark);
    void fill_translucent(const core::LayoutRect &area, float radius_dips, core::RgbaColor color);
    void paint_header(const core::SettingsLayout &layout);
    void paint_search(const application::SettingsView &view, const core::SettingsLayout &layout);
    void paint_chips(const application::SettingsView &view, const core::SettingsLayout &layout);
    void paint_items(const application::SettingsView &view, const core::SettingsLayout &layout);
    void paint_item(const application::SettingsView &view, const core::SettingsLayout &layout,
                    const core::SettingsLayoutItem &item);
    void paint_summary(const application::SettingsView &view, const core::SettingsLayout &layout,
                       const core::LayoutRect &box);
    void paint_row(const application::SettingsRowView &row, const core::SettingsLayout &layout,
                   const core::SettingsLayoutItem &item);
    void paint_tier(const application::SettingsRowView &row, const core::LayoutRect &area);
    void paint_control(const application::SettingsRowView &row, const core::SettingsLayout &layout,
                       const core::SettingsLayoutItem &item, const core::LayoutRect &box);
    void paint_switch(const core::LayoutRect &box, bool on);
    void paint_value(std::string_view value, const core::LayoutRect &box, bool chosen);
    void paint_stepper(std::string_view value, const core::SettingsLayout &layout,
                       const core::LayoutRect &box);
    void paint_any_row(const application::SettingsView &view, const core::SettingsLayout &layout,
                       const core::SettingsLayoutItem &item);
    [[nodiscard]] std::pair<core::UiMessage, core::RgbColor>
    server_state(application::LanguageServerRowState state) const noexcept;
    void paint_server(const application::LanguageServerRowView &row,
                      const core::SettingsLayout &layout, const core::SettingsLayoutItem &item);
    void paint_server_button(application::LanguageServerRowAction action,
                             const core::LayoutRect &box);
    [[nodiscard]] static bool tail_matters(application::LanguageServerRowState state) noexcept;
    void paint_fold(const application::SettingsView &view, const core::LayoutRect &box);
    void paint_note(core::UiMessage message, const core::LayoutRect &box);
    void paint_footer(const application::SettingsView &view, const core::SettingsLayout &layout);
    void paint_button(std::string_view label, const core::LayoutRect &box, bool filled,
                      core::RgbColor ink);

    [[nodiscard]] std::string_view said(core::UiMessage message) const noexcept;
    // 1 行の広い箱の文字組み（保持から引く）と、その幅（物理画素・切り上げ）。
    [[nodiscard]] PaletteRowLayouts::TextLayout line(std::string_view text,
                                                     IDWriteTextFormat *format) const;
    // 幅 limit の箱の文字組み（入り切らない末尾を「…」で切る・DirectWrite の trimming・保持から
    // 引く）。字の途中で切れた文を出さない（#1836 のレビュー R3）。
    [[nodiscard]] PaletteRowLayouts::TextLayout
    trimmed(std::string_view text, IDWriteTextFormat *format, std::int32_t limit) const;
    // 幅 limit で折り返す 2 行ぶんの箱の文字組み（要約の 1 行目が入り切らないとき）。
    [[nodiscard]] PaletteRowLayouts::TextLayout
    wrapped(std::string_view text, IDWriteTextFormat *format, std::int32_t limit) const;
    [[nodiscard]] PaletteRowLayouts::TextLayout composed(std::string_view text,
                                                         IDWriteTextFormat *format,
                                                         const core::LayoutRect &area,
                                                         bool trim) const;
    [[nodiscard]] std::int32_t width(std::string_view text, IDWriteTextFormat *format) const;
    // area の左端から描く。area に入り切らなければ末尾を「…」で切る。描いた文字の幅を返す。
    std::int32_t write(std::string_view text, IDWriteTextFormat *format,
                       const core::LayoutRect &area, core::RgbColor color);
    // area の幅で折り返して描く（2 行まで・縦は area の中央）。
    void write_wrapped(std::string_view text, IDWriteTextFormat *format,
                       const core::LayoutRect &area, core::RgbColor color);
    // area に入り切らなければ先頭側を「…」で省いて末尾を残す（パスの実行ファイルの名前・#1945）。
    void write_tail(std::string_view text, IDWriteTextFormat *format, const core::LayoutRect &area,
                    core::RgbColor color);
    void write_centered(std::string_view text, IDWriteTextFormat *format,
                        const core::LayoutRect &area, core::RgbColor color);
    void write_trailing(std::string_view text, IDWriteTextFormat *format,
                        const core::LayoutRect &area, core::RgbColor color);
    void fill(const core::LayoutRect &area, core::RgbColor color);
    void fill_rounded(const core::LayoutRect &area, float radius_dips, core::RgbColor color);
    void outline(const core::LayoutRect &area, float radius_dips, core::RgbColor color);
    void stroke(D2D1_POINT_2F from, D2D1_POINT_2F to, core::RgbColor color);
    [[nodiscard]] std::int32_t pixels(std::int32_t dips) const noexcept;
    [[nodiscard]] float scaled(float dips) const noexcept;

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    SettingsPanelText text_;
    PaletteRowLayouts &layouts_;
    core::Palette palette_{};
    core::RgbColor heavy_{};
    std::optional<core::SettingsChoiceLayout> painted_choice_;
};
} // namespace nenenib::ui::win32
