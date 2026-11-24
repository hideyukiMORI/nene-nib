#pragma once

#include "EditorIntent.hpp"
#include "SettingsKeyPress.hpp"
#include "SettingsLayout.hpp"
#include "SettingsPart.hpp"
#include "SettingsPress.hpp"
#include "SettingsTarget.hpp"
#include "SettingsView.hpp"

#include <windows.h>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
// 設定の面が開いている間の鍵・文字・マウスを面の意図の列に写す純関数（施主決定 D102・#1836 の
// 差し戻し R5）。窓の手続き（EditorWindowSettings.cpp）は値を集めてここを呼び、返った意図を順に
// 送るだけ。窓を作らずに試せる（nib_window_tests の `--settings-panel`）。返す意図は面の意図
// （application の is_settings_intent の集合）だけで、本文を変える意図は 1 つも返さない。
using SettingsIntents = std::vector<application::EditorIntent>;

// 選ぶ欄の一覧が開いている間（view.choice）の鍵と文字は一覧へ（settings_choice_key_action・↑ ↓ は
// 端で止まる・Space は決める・字は頭文字の跳び・#1926）。
// WM_KEYDOWN。鍵の表（settings_key_action / settings_section_key）の行き先を、今の view と最後に
// 描いた配置 layout（PageUp / PageDown の行き先・描く前は 1 行だけ動く）から意図にする。Esc と
// Ctrl+, は CloseSettings の 1 つ（長押しの反復では閉じない＝開閉を繰り返さない）。
[[nodiscard]] SettingsIntents
settings_key_intents(const SettingsKeyPress &press, const application::SettingsView &view,
                     const std::optional<core::SettingsLayout> &layout);

// WM_CHAR（IME の確定を含む）。文字は探す欄へ、探す欄が空のときの Space は焦点の行の切り替え。
// Ctrl つきの文字（Ctrl+Space の 0x20 など）と制御文字は受けない（本文の type_character と同じ
// 守り・R1）。pending は上位サロゲートの覚え（呼ぶ側が持つ）。
[[nodiscard]] SettingsIntents settings_character_intents(wchar_t unit, bool control,
                                                         wchar_t &pending,
                                                         const application::SettingsView &view);

// 左の押し。面の外は CloseSettings の 1 つ（本文へ渡さない）で覚えは無し、面の中は部品と、言語
// サーバーの行なら行の身元を覚えるだけ（view は押したときの表示値）。
[[nodiscard]] std::pair<std::optional<SettingsPress>, SettingsIntents>
settings_press_step(core::SettingsTarget target, const application::SettingsView &view);
// 左の離し。押した部品と同じ部品で、言語サーバーの行なら同じ身元の行の上で離したときだけ、その
// 部品の意図（押している間に探しの結果が届いて行がずれたら何もしない・#1945 のレビュー R2）。
[[nodiscard]] SettingsIntents settings_release_intents(const std::optional<SettingsPress> &pressed,
                                                       core::SettingsTarget released,
                                                       const application::SettingsView &view);
// 右の押し。面の外は CloseSettings の 1 つ、中は何もしない（どちらも本文のメニューを開かない）。
[[nodiscard]] SettingsIntents settings_right_press_intents(core::SettingsTarget target);

// WM_SYSKEYDOWN（Alt つきの鍵・#1926）。Alt+↓ は焦点の行の選ぶ欄の一覧を開き、Alt+↑ は開いている
// 一覧を決めずに閉じる（Windows のコンボボックスの標準）。ほかの鍵は値なし（OS の既定処理へ渡す・
// Alt+F4 を止めない）。
[[nodiscard]] std::optional<SettingsIntents>
settings_system_key_intents(WPARAM key, const application::SettingsView &view);
// 一覧が開いている間の左の押し。inside は一覧の矩形の中か、choice はその下の選択肢。一覧の外は
// CloseSettingsChoice の 1 つ（その押しはほかの物を動かさない）、行の上はその行を選ぼうとしている
// 行にして押した行を覚える、一覧の上下の空きは何もしない。
[[nodiscard]] std::pair<std::optional<std::size_t>, SettingsIntents>
settings_choice_press_step(bool inside, std::optional<std::size_t> choice);
// 一覧の上の左の離し。押した行と同じ行で離したときだけ、その行に決める。
[[nodiscard]] SettingsIntents settings_choice_release_intents(std::optional<std::size_t> pressed,
                                                              std::optional<std::size_t> released);

// ホイールを面が取るか。面の上だけ（行の領域を送る）で、面の外（本文・帯・エクスプローラー）の
// ホイールは今の道へ渡す（application は送るだけの意図で面を閉じない）。
[[nodiscard]] constexpr bool settings_takes_wheel(core::SettingsTarget target) noexcept
{
    return target.part != core::SettingsPart::outside;
}

// 面がこの意図で閉じたか。閉じた所で IME の変換の取り消しを 1 回呼ぶ（探す欄で変換中のまま閉じても
// 続きが本文へ入らない・レビュー M1・入力行の ADR 0061 決定 3 と同じ形）。閉じる道（外のクリック・
// ✕・「閉じる」・Esc・Ctrl+,・application が閉じた意図）はどれもこの 1 か所を通る。
[[nodiscard]] constexpr bool settings_just_closed(bool was_open, bool open) noexcept
{
    return was_open && !open;
}
} // namespace nenenib::ui::win32
