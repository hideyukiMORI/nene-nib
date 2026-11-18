#pragma once

#include "LayoutRect.hpp"
#include "SettingsControl.hpp"
#include "SettingsLayout.hpp"
#include "SettingsLayoutItem.hpp"
#include "SettingsLayoutRequest.hpp"
#include "SettingsPreset.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsTarget.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 設定の面の配置と当たり（施主決定 D102 の右の引き出し・ADR 0170 の決定 5・#1836）。形の選択
// （引き出しか全幅か）はこの中だけで、描画と当たりは同じ settings_layout の値を読む。寸法は
// 採用案の見本（docs/design/2026-11-18-settings-modal-look.md）の値。

// 引き出しの幅と、全幅に切り替える窓の幅（DIP）。
inline constexpr std::int32_t settings_panel_width_dips = 460;
inline constexpr std::int32_t settings_narrow_below_dips = 700;
// 行の高さ・スイッチ・選ぶ欄と段の上下の大きさ（DIP）。
inline constexpr std::int32_t settings_row_height_dips = 56;
inline constexpr std::int32_t settings_switch_width_dips = 40;
inline constexpr std::int32_t settings_switch_height_dips = 22;
inline constexpr std::int32_t settings_value_width_dips = 172;
inline constexpr std::int32_t settings_value_height_dips = 30;
// 切れない機能の「常にオン」の語の欄の幅（DIP・高さはスイッチと同じ）。
inline constexpr std::int32_t settings_fixed_width_dips = 72;
// 行の内側の左右の余白と、行の中の部品どうしの空き（DIP）。
inline constexpr std::int32_t settings_row_padding_dips = 12;
inline constexpr std::int32_t settings_row_gap_dips = 14;
// ホイールの 1 行の送り（DIP）。
inline constexpr std::int32_t settings_wheel_line_dips = 20;

// 節の札と見出しの順（宣言の順）。
inline constexpr std::array<SettingsSection, 6> settings_sections{
    SettingsSection::display, SettingsSection::text,     SettingsSection::editing,
    SettingsSection::panes,   SettingsSection::features, SettingsSection::language_server};
// 組のボタンの順。
inline constexpr std::array<SettingsPreset, 3> settings_presets{
    SettingsPreset::speed_first, SettingsPreset::recommended, SettingsPreset::all_on};

// 行の右端の部品。機能は切れればスイッチ・切れなければ「常にオン」、値は字の大きさが段の上下・
// 字体が読むだけ・ほかは選ぶ欄。
[[nodiscard]] SettingsControl settings_control_of(const SettingsSubject &subject,
                                                  bool switchable) noexcept;

// 面の矩形。area の右端に幅 460 DIP で置き、area の幅が 700 DIP 未満なら全幅。行の領域は見出し・
// 探す欄・札と下の帯の間で、行が入り切らなければ request.scroll だけ上へ送る（0 から上限へ
// 切り詰める）。
[[nodiscard]] SettingsLayout settings_layout(const SettingsLayoutRequest &request);

// 点の下の部品。行の領域の中の部品（組・行）は領域に見えている所だけ当たる。
[[nodiscard]] SettingsTarget settings_hit(const SettingsLayout &layout, std::int32_t x,
                                          std::int32_t y) noexcept;

// 行の右端の部品の矩形（スイッチは 40 × 22・「常にオン」の語は 72 × 22・ほかは 172 × 30・行の右の
// 内側に縦の中央）。
[[nodiscard]] LayoutRect settings_control_box(const SettingsLayout &layout,
                                              const SettingsLayoutItem &row) noexcept;
// 段の上下の欄 box の中の −（plus が偽）と ＋ のボタン。
[[nodiscard]] LayoutRect settings_step_box(const SettingsLayout &layout, const LayoutRect &box,
                                           bool plus) noexcept;

// 見えている行 row が行の領域に入る送り（最小の動き）。節の最初の行は見出し（と要約）も入れる。
// 行が無ければ今の送り。
[[nodiscard]] std::int32_t settings_scroll_revealing(const SettingsLayout &layout,
                                                     std::size_t row) noexcept;
// ホイールで lines 行（正が下）送った送り。上限で止まる。
[[nodiscard]] std::int32_t settings_scroll_by(const SettingsLayout &layout,
                                              std::int32_t lines) noexcept;
// PageUp / PageDown の行き先（見えている行の中の位置）。焦点から行の領域の高さぶん先の行で、端で
// 止まる。焦点が無ければ最初の行。行が無ければ値なし。
[[nodiscard]] std::optional<std::size_t> settings_paged_row(const SettingsLayout &layout,
                                                            std::optional<std::size_t> focused,
                                                            bool down) noexcept;
} // namespace nenenib::core
