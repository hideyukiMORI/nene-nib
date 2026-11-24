#pragma once

#include "DisplayLanguage.hpp"
#include "EditorSettings.hpp"
#include "LayoutRect.hpp"
#include "SettingsChoiceLayout.hpp"
#include "SettingsChoiceRequest.hpp"
#include "SettingsValue.hpp"
#include "ThemeChoice.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// ---------------------------------------------------------------- 選択肢の列（#1926）

// 選ぶ欄（SettingsControl::choice）の選択肢の列。順は settings_value_stepped が 1
// つずつ送る列と同じ 正本（配色は system → 組み込みの表の順 → 一覧に無い配色（今の値か kept
// が利用者のテーマなら）・ 表示言語は listed_display_language_choices・ほかは SettingsRows.cpp
// の送りの順の表）。choice で ない値（段の上下と字体）は空の列（数 0）。kept
// には面を開いたときの配色を渡す（#1835 の R5）。
[[nodiscard]] std::size_t settings_choice_count(const EditorSettings &settings, SettingsValue value,
                                                const std::optional<ThemeChoice> &kept) noexcept;
// 今の値の位置。列に無い値（列の無い表示言語など）と choice でない値は値なし。
[[nodiscard]] std::optional<std::size_t>
settings_choice_position(const EditorSettings &settings, SettingsValue value,
                         const std::optional<ThemeChoice> &kept) noexcept;
// 位置 position の選択肢に決めた設定（列の外と choice でない値はそのまま返す）。
[[nodiscard]] EditorSettings settings_choice_applied(EditorSettings settings, SettingsValue value,
                                                     std::size_t position,
                                                     const std::optional<ThemeChoice> &kept);
// 選択肢の表示の名前の列（settings_value_text がその値に付ける名前と同じ）。
[[nodiscard]] std::vector<std::string> settings_choice_texts(const EditorSettings &settings,
                                                             SettingsValue value,
                                                             const std::optional<ThemeChoice> &kept,
                                                             DisplayLanguage language);
// 頭文字の跳び: 名前が typed で始まる最初の選択肢（前の方から・ASCII の大小は区別しない）。typed が
// 空か当たりが無ければ値なし。
[[nodiscard]] std::optional<std::size_t>
settings_choice_typed(const std::vector<std::string> &texts, std::string_view typed) noexcept;

// ---------------------------------------------------------------- 一覧の配置（#1926）

// 寸法（DIP）。行の高さと上下の内側の空きは右クリックのメニューと同じ（context_menu_row_dips・
// context_menu_gap_dips）。見える行は最大 10。印（✓）の欄の幅・左右の内側・送りの棒の幅は採用案の
// 見本の値。
inline constexpr std::size_t settings_choice_rows = 10;
inline constexpr std::int32_t settings_choice_mark_dips = 24;
inline constexpr std::int32_t settings_choice_padding_dips = 10;
inline constexpr std::int32_t settings_choice_bar_dips = 4;

// 一覧の矩形と描く行（純関数）。値の箱の下に、右端を箱にそろえて出す。下に行が全部入らなければ上、
// どちらにも入らなければ広い側に入るだけの行（1 行以上）。幅は「箱の幅」と「いちばん長い選択肢 ＋
// 印の欄 ＋ 内側」の大きい方で、bounds の幅まで。first は送り top を「最後の rows
// 行が見える」所まで 切り詰めた値。
[[nodiscard]] SettingsChoiceLayout
settings_choice_layout(const SettingsChoiceRequest &request) noexcept;
// 一覧の index 番目（0 始まり・layout.rows 未満）の行の矩形と、その中の印の欄・名前の欄。
[[nodiscard]] LayoutRect settings_choice_row(const SettingsChoiceLayout &layout,
                                             std::size_t index) noexcept;
[[nodiscard]] LayoutRect settings_choice_mark(const SettingsChoiceLayout &layout,
                                              std::size_t index) noexcept;
[[nodiscard]] LayoutRect settings_choice_label(const SettingsChoiceLayout &layout,
                                               std::size_t index) noexcept;
// 送りの棒のつまみ（選択肢が全部見えていれば幅 0）。
[[nodiscard]] LayoutRect settings_choice_thumb(const SettingsChoiceLayout &layout) noexcept;
// 点（クライアント座標の物理画素）の下の選択肢の位置（layout.first + 行）。一覧の外と上下の内側の
// 空きと送りの棒の欄は値なし（一覧の中かは contains(layout.panel, …)
// で引く）。描画と窓の当たりが同じ配置を読む。
[[nodiscard]] std::optional<std::size_t>
settings_choice_hit(const SettingsChoiceLayout &layout, std::int32_t x, std::int32_t y) noexcept;
// 位置 at が見える送り（見えていれば layout.first のまま）。
[[nodiscard]] std::size_t settings_choice_revealing(const SettingsChoiceLayout &layout,
                                                    std::size_t at) noexcept;
// 開いた直後の送り: 位置 at が見える範囲（最大 settings_choice_rows 行）の中ほどに来る送り。
[[nodiscard]] std::size_t settings_choice_centred(std::size_t count, std::size_t at) noexcept;
// PgUp / PgDn の 1 歩の行き先（見えている行の数 rows - 1 ずつ・端で止まる）。rows が 0（まだ
// 描いていない）なら最大の行の数で数える。
[[nodiscard]] std::size_t settings_choice_paged(std::size_t count, std::size_t at, bool down,
                                                std::size_t rows) noexcept;
// 一覧を送り from からホイールで lines 行送った送り（最後の rows 行が見える所で止まる・変わら
// なければ値なし）。from は application の状態の送り（描く前に 2 通来ても 1 通目が消えない）。
[[nodiscard]] std::optional<std::size_t>
settings_choice_scrolled(const SettingsChoiceLayout &layout, std::size_t from,
                         std::int32_t lines) noexcept;
} // namespace nenenib::core
