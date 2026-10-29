#pragma once

#include "Appearance.hpp"
#include "DisplayLanguageChoice.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "GuideVisibility.hpp"
#include "LineWrapping.hpp"
#include "MapRow.hpp"
#include "Theme.hpp"
#include "ThemeChoice.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::core
{
// 保存する設定（設定 v4 の鍵の集合・ADR 0170 の決定 3）。
// ui_language・lsp・features はいまは読み書きするだけで、使う所は後の PR
// （ADR 0123 の改訂・ADR 0160・ADR 0170）。explorer は左の面の表示、root は
// エクスプローラーのルートで、無しは「決めていない」（ADR 0162 の決定 3）。
// features はオンの機能の名前（ASCII）の列で、空は「表の既定」。中身はここでは
// 解釈しない。lsp_commands は言語ごとのサーバーの命令（言語の名前と値・言語の
// 名前の順・ADR 0160）で、値の検証は使う所（L0c）がする。maps は Vim の `map` の行（読んだ順・
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
};

[[nodiscard]] EditorSettings default_editor_settings();
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
