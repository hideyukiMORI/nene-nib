#pragma once

#include "Appearance.hpp"
#include "DisplayLanguageChoice.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "FinderPreview.hpp"
#include "FontSize.hpp"
#include "GuideVisibility.hpp"
#include "IndentGuideStyle.hpp"
#include "LineWrapping.hpp"
#include "MapRow.hpp"
#include "Theme.hpp"
#include "ThemeChoice.hpp"
#include "VimPatternCase.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
// 保存する設定（設定 v4 の鍵の集合・ADR 0170 の決定 3）。
// ui_language は表示言語の希望値（ADR 0123 の 2026-11-13 の追補・解決は resolved_language）。
// features はいまは読み書きするだけで、使う所は後の PR（ADR 0170）。explorer は左の面の表示、root
// は エクスプローラーのルートで、無しは「決めていない」（ADR 0162 の決定 3）。 features
// はオンの機能の名前（ASCII）の列で、空は「表の既定」。中身はここでは 解釈しない。lsp_commands
// は言語ごとのサーバーの命令（言語の名前と値・言語の 名前の順・ADR
// 0160）で、値の検証は使う所（L0c）がする。表に無い名前の行（後の版の言語）も持って書き戻す
// （使わない・ADR 0170 の 2026-11-20 の追補）。maps は Vim の `map` の行（読んだ順・
// 同じモードと左辺は後の行が勝つ）、mapleader は `<leader>` の鍵の記法（既定は `\`）で、表は
// key_map_of が作る（ADR 0176 の決定 1・2）。
struct EditorSettings
{
    FontSize font_size;
    DisplayText font_family;
    // 無しはsystem。選択済みのテーマは生存する配色を共有する（ADR 0025）。
    std::optional<ThemeChoice> theme;
    GuideVisibility guide;
    LineWrapping wrapping = LineWrapping::wrap;
    DisplayLanguageChoice ui_language = DisplayLanguageChoice::system;
    bool lsp = false;
    bool explorer = false;
    std::optional<FilePath> root = std::nullopt;
    std::vector<std::string> features = {};
    std::vector<std::pair<std::string, std::string>> lsp_commands = {};
    std::vector<MapRow> maps = {};
    std::string mapleader = "\\";
    // 検索の大小（Vim の `ignorecase` `smartcase`・既定はどちらもオフ・ADR 0032 の追補・#1041）。
    // `:set` だけが変え、設定の面には出さない。
    bool ignorecase = false;
    bool smartcase = false;
    // 行番号の相対表示と追従の余白（Vim の `relativenumber` `scrolloff`・既定はオフと 0・
    // #1041）。`:set` だけが変え、設定の面には出さない。scrolloff は 0〜999（scrolloff_limit）。
    bool relativenumber = false;
    std::size_t scrolloff = 0;
    // エクスプローラーの面の幅（DIP・境のドラッグで決める・ADR 0162 の追補・施主決定 D78）。無しは
    // 既定の式 min(240 DIP, client の 40%)。値は explorer_width_of が読める範囲で、描く幅は
    // explorer_panel_width が窓に合わせて切り詰める（ここは書き換えない）。
    std::optional<std::int32_t> explorer_width = std::nullopt;
    // INSERT の改行と `o` `O` `cc` `S` が字下げを写すか（Vim の `autoindent`・既定はオフ・#1086）。
    // `:set` だけが変え、設定の面には出さない（ignorecase などと同じ束）。
    bool autoindent = false;
    // 探して見る面のプレビュー欄の置き方（ADR 0189 の決定 1・settings.v5 の鍵 `finder_preview`・
    // 既定は automatic）。`:set finderpreview=` と面が開いている間の Ctrl+Shift+P が変える。
    FinderPreview finder_preview = FinderPreview::automatic;
    // キャレットの動き（尾を引く・ADR 0192・#1933・settings.v6 の鍵 `caret_motion` と
    // `caret_motion_time`）。既定はオフと 120 ms（施主の決め 2026-11-22）。時間は
    // caret_motion_time_minimum〜caret_motion_time_maximum（CaretGlide.hpp）。
    bool caret_motion = false;
    std::uint16_t caret_motion_time = 120;
    // 打ったときの粒（ADR 0192 の追補・#1979・settings.v6 の鍵 `caret_sparks`）。既定はオフ（施主の
    // 決め 2026-11-23）。キャレットの動き（caret_motion）と独立に入れられる。
    bool caret_sparks = false;
    // 枠の「広げる / 狭める」の 1 回分の升の数と、Vim の回数なしの `Ctrl-w > < + -` もそれに従うか
    // （ADR 0148 の 2026-11-23 の追補・#1972・settings.v6 の鍵 `pane_resize_step` と
    // `pane_resize_step_vim`）。既定は 5 升と off（off の Vim の鍵は本物の Vim と同じ 1 升）。升の
    // 数は pane_resize_step_minimum〜pane_resize_step_maximum（PaneResizeStep.hpp）。
    std::uint8_t pane_resize_step = 5;
    bool pane_resize_step_vim = false;
    // インデントガイドの形（ADR 0193 の決定 4・#1975・settings.v6 の鍵 `indent_guide_style`
    // `indent_guide_colors` `indent_guide_active` `indent_guide_width`）。表示のオン・オフは
    // `features=indentguides`。色の数は 3〜6、段の幅は 2 / 4 / 8 で推し当てが外れた文書に使う
    // （IndentGuide.hpp）。
    IndentGuideStyle indent_guide_style = IndentGuideStyle::rainbow;
    std::uint8_t indent_guide_colors = 4;
    bool indent_guide_active = true;
    std::uint8_t indent_guide_width = 4;
};

// `scrolloff` の上限（Vim の値は任意の非負だが、画面の半分で切り詰めるので 999 で足りる）。
inline constexpr std::size_t scrolloff_limit = 999;
// `scrolloff` の値の文字列（10 進・先頭の 0 なし・0〜scrolloff_limit）を読む 1 本。Ex と設定の
// 読みが共用する（#1041）。
[[nodiscard]] std::optional<std::size_t> scrolloff_of(std::string_view text) noexcept;

[[nodiscard]] EditorSettings default_editor_settings();
// 設定の `ignorecase` `smartcase` から照合器の旗へ（#1041）。smartcase は ignorecase が
// オンのときだけ効く（Vim と同じ）。
[[nodiscard]] VimPatternCase search_case(const EditorSettings &settings) noexcept;
[[nodiscard]] bool same_settings(const EditorSettings &left, const EditorSettings &right) noexcept;
// 字の大きさか字体が変わると本文の行高と文字組みが変わる。窓は新しい行高の幾何を送り直す
// （ADR 0128 の決定 8）。判定はこの 1 本で、呼び手で欄を数えない。
[[nodiscard]] bool affects_line_height(const EditorSettings &before,
                                       const EditorSettings &after) noexcept;
[[nodiscard]] Theme selected_theme(const EditorSettings &settings,
                                   Appearance system_appearance) noexcept;
Theme selected_theme(EditorSettings &&, Appearance) = delete;
Theme selected_theme(const EditorSettings &&, Appearance) = delete;
} // namespace nenenib::core
