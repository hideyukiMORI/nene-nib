#pragma once

#include "DisplayLanguage.hpp"
#include "EditorFeature.hpp"
#include "EditorSettings.hpp"
#include "SettingsRow.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsValue.hpp"
#include "ThemeChoice.hpp"
#include "UiMessage.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 設定の面の項目の列（ADR 0170 の決定 5・2026-11-18 の追補 2・#1016 の段 1）。節の順に上から。
// 機能の行は feature_costs の表の切れる機能と、切れないが段の印のある機能（matchparen・「常に
// オン」の行）。切れず印も無い機能（easymotion・start_screen）と per_document の syntax は載せない
// （#1835 のレビュー R4）。面の行はこの表の 1 つから作り、ui は frame の値を読むだけ。
// 折り返しは「編集」の節、言語サーバーのオン・オフは「機能」の節（要約と組の下に印のある行が並ぶ・
// #1836 の差し戻し）。「言語サーバー」の節は行を持たず、言語ごとのコマンド（読むだけ）だけが並ぶ。
inline constexpr std::array<SettingsRow, 12> settings_rows{{
    {SettingsSection::display, SettingsValue::theme},
    {SettingsSection::display, SettingsValue::ui_language},
    {SettingsSection::display, EditorFeature::guide},
    {SettingsSection::text, SettingsValue::font_family},
    {SettingsSection::text, SettingsValue::font_size},
    {SettingsSection::editing, EditorFeature::wrap},
    {SettingsSection::editing, EditorFeature::autoindent},
    {SettingsSection::panes, EditorFeature::explorer},
    {SettingsSection::panes, SettingsValue::finder_preview},
    {SettingsSection::features, EditorFeature::matchparen},
    {SettingsSection::features, EditorFeature::minimap},
    {SettingsSection::features, EditorFeature::lsp},
}};

// 節の見出しの文。
[[nodiscard]] UiMessage settings_section_title(SettingsSection section) noexcept;
// 行の名前と 1 行の説明の文（機能は feature_costs の行・値は表の外の決まった文）。
[[nodiscard]] UiMessage settings_row_title(const SettingsSubject &subject) noexcept;
[[nodiscard]] UiMessage settings_row_description(const SettingsSubject &subject) noexcept;
// 行の `:set` の名前（機能は feature_name_of・値は `colorscheme` `language` `guifont`
// `fontsize` `finderpreview`）。探す欄が名前・説明と同じく照らす。
[[nodiscard]] std::string_view settings_row_set_name(const SettingsSubject &subject) noexcept;

// 探す欄（追補 2）。query が空なら全部の行。そうでなければ `:set` の名前・表示言語の名前・説明の
// どれかが query を含む行（ASCII の大小は区別しない）。
[[nodiscard]] bool settings_row_matches(const SettingsRow &row, std::string_view query,
                                        DisplayLanguage language) noexcept;

// 値の行の表示の文字（表示言語 language の名前・設定のファイルの生の値は出さない・#1836 の
// 差し戻し）。配色は組み込みと利用者のテーマの名前そのまま・選んでいなければ「Windows に
// 合わせる」。表示言語は言語の自称・system は「Windows に合わせる」。字体と pt はそのまま。
// プレビューの置き方は「自動」「右」「下」「出さない」。
[[nodiscard]] std::string settings_value_text(const EditorSettings &settings, SettingsValue value,
                                              DisplayLanguage language);
// 値を 1 つ送った設定（forward は次・大きい側）。配色の列は system → 組み込みの表の順 → 一覧に
// 無い配色（今の値が利用者のテーマならそれ、でなければ kept が利用者のテーマならそれ・無ければ
// 列に入れない）で、回り込む。今の値を列の 1 要素として扱うので、利用者のテーマから送っても戻れる
// （kept には面を開いたときの配色を渡す・#1835 のレビュー R5）。表示言語と置き方は宣言の順で回り
// 込む。字の大きさは 1 pt ずつで端で止まる。字体は段 1 では送らない（そのまま返す）。
[[nodiscard]] EditorSettings settings_value_stepped(EditorSettings settings, SettingsValue value,
                                                    bool forward,
                                                    const std::optional<ThemeChoice> &kept);
} // namespace nenenib::core
