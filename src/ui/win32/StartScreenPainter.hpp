#pragma once

#include "FilePath.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "PaletteTextRole.hpp"
#include "RgbColor.hpp"
#include "StartEntry.hpp"
#include "StartRowLayout.hpp"
#include "StartScreenLayout.hpp"
#include "StartScreenText.hpp"
#include "StartScreenView.hpp"

#include <cstddef>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
// 起動画面を描く経路（ADR 0172 の決定 5・追補 2026-11-11 の 17〜19・採用案
// docs/design/2026-10-28-start-screen-look.md）。本文の面（本文の地と行番号の上）の中央寄せの 1 列
// に、題「NeNe Nib」・節の見出し（muted）・選んだ行（current_line で列の全幅・先頭に ▸ を muted）を
// 描く。最近使ったファイルとブックマークの行は名前（text）とその右の場所（muted・入り切らなければ
// 頭側を「…」で切る）で、名前と場所は Ctrl+P の面と同じ tab_title_for / tab_folder_for から作る。
// 「はじめる」の行は名前のすぐ後ろに補足（muted）、「ファイルを開く」は右端に鍵の枠。ショートカット
// の節は鍵の枠をほかの行の項目と同じ左端に揃え、説明をその右に置く。
// 配置は core::start_screen_layout の 1 本（窓のクリックの当たりと同じ）で、名前の幅で決まる
// 場所と補足の左端だけを測って置く（Ctrl+P の面の場所と同じ）。行の文字組みは
// PaletteRowLayouts の保持から引き、同じ画面は frame をまたいで作り直さない（#904 の形）。
// target は製品では Direct2D の描画先、tests/ui は WIC の bitmap の描画先。
// 色は Palette のトークンだけ（ADR 0008）。
class StartScreenPainter final
{
  public:
    // 保持の上限。題 1・見出し 4・空の知らせ 1・印 1・行 23（名前と場所か補足・場所の全文と頭を
    // 切った文）・「…」1・鍵 4 組で 128 に収まる。
    static constexpr std::size_t retained = 128;

    StartScreenPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                       const StartScreenText &text, PaletteRowLayouts &layouts) noexcept;
    // 画面を描く。clip は本文の帯（列が帯の下へはみ出す分を切る）。
    void paint(const application::StartScreenView &view, const core::StartScreenLayout &layout,
               const core::Palette &palette, const core::LayoutRect &clip);

  private:
    void fill(const core::LayoutRect &area, core::RgbColor color);
    // 書式と文字組みの焼き方の組。
    using TextStyle = std::pair<IDWriteTextFormat *, PaletteTextRole>;
    // 欄に書き、書いた字の幅（物理画素・後ろの空白を含む）を返す。書けなければ欄の幅。
    float write(std::string_view text, TextStyle style, const core::LayoutRect &area,
                core::RgbColor color);
    // 鍵の枠。右寄せは欄の右端に、左寄せは欄の左端に揃える。
    void keycap(std::string_view key, const core::LayoutRect &area, bool from_left);
    void paint_headings(const core::StartScreenLayout &layout, const core::Palette &palette);
    void paint_row(const core::StartEntry &entry, const core::StartRowLayout &row, bool selected,
                   const core::Palette &palette);
    // 名前（text）と、そのすぐ後ろの補足（muted・空なら書かない）。
    void paint_named(std::string_view name, std::string_view detail, const core::LayoutRect &area,
                     const core::Palette &palette);
    // ファイルの名前（text）とその右の場所（muted・頭側を切る）。
    void paint_path(const core::FilePath &path, const core::LayoutRect &area,
                    const core::Palette &palette);
    // 欄に入るように頭側を「…」で切った文（入れば全文）。
    [[nodiscard]] std::string head_trimmed(std::string_view text, const core::LayoutRect &area);
    void paint_keys(const application::StartScreenView &view, const core::StartScreenLayout &layout,
                    const core::Palette &palette);
    [[nodiscard]] PaletteRowLayouts::TextLayout made(std::string_view text, TextStyle style,
                                                     const core::LayoutRect &area);
    [[nodiscard]] PaletteRowLayouts::TextLayout obtained(std::string_view text, TextStyle style,
                                                         const core::LayoutRect &area);

    ID2D1RenderTarget &target_;
    ID2D1SolidColorBrush &brush_;
    StartScreenText text_;
    PaletteRowLayouts &layouts_;
};
} // namespace nenenib::ui::win32
