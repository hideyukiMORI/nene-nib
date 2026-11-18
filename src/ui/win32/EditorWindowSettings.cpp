// 設定の面（右の引き出し）の窓の側（施主決定 D102・ADR 0170 の決定 5・6・#1836）。開いている間の
// 鍵と文字は面へ送り、本文へ通さない（通すと application が面を閉じて保存する）。何が変わるか・
// いつ保存するかは controller が決め、窓は意図を送るだけ（ARC-011）。鍵・文字・マウスを意図の列に
// 写すのは SettingsInput の純関数で、ここは値を集めて送るだけ。当たりは renderer が最後に描いた
// 配置（core::settings_layout の 1 本）から引く。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "DevicePixels.hpp"
#include "EditorIntent.hpp"
#include "JumpSettingsSection.hpp"
#include "LayoutRect.hpp"
#include "OpenSettings.hpp"
#include "ScrollSettings.hpp"
#include "SettingsInput.hpp"
#include "SettingsKeyPress.hpp"
#include "SettingsLayout.hpp"
#include "SettingsPart.hpp"
#include "SettingsTarget.hpp"
#include "WheelTurn.hpp"

#include <imm.h>
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::ui::win32
{
using detail::held;
using detail::high_word_of;
using detail::low_word_of;

namespace
{
// 1 画面の設定のホイールの 1 刻みで送る行の数。
constexpr std::size_t page_wheel_lines = 10;
// 変換の窓を探す欄の中に置く位置（虫めがねの右・DIP）。
constexpr std::int32_t composition_left_dips = 34;
constexpr std::int32_t composition_top_dips = 8;

} // namespace

// 操作「設定」（Ctrl+, と F1 の一覧）。開いている間の Ctrl+, は面の鍵が先に受けて閉じる。
void EditorWindow::run_open_settings()
{
    // 本文の変換中に開いたら、IME の側の変換も取り消す（状態の変換は application が消す・#1835 の
    // 再確認 L-b2）。
    if (controller_.is_composing())
    {
        cancel_ime_composition();
    }
    send(application::OpenSettings{});
}

// 面の意図を送り、焦点の行が行の領域の外なら見える所へ送りを寄せる。
void EditorWindow::send_settings(const application::EditorIntent &intent)
{
    send(intent);
    if (renderer_ == nullptr)
    {
        return;
    }
    const auto view = controller_.settings_view();
    if (!view.has_value())
    {
        return;
    }
    // 行を持たない節（言語サーバー）の札と Ctrl+6 は、焦点が動かないので節の見出しへ送る。
    const auto *jump = std::get_if<application::JumpSettingsSection>(&intent);
    const auto section = jump != nullptr && view.value().section != std::optional{jump->section}
                             ? std::optional{jump->section}
                             : std::nullopt;
    const auto top =
        renderer_->settings_revealed(view.value(), controller_.display_language(), section);
    if (top.has_value())
    {
        send(application::ScrollSettings{top.value()});
    }
}

bool EditorWindow::take_settings_key(UINT message, WPARAM word, LPARAM data)
{
    const auto view = controller_.settings_view();
    if (!view.has_value())
    {
        return false;
    }
    const bool control = held(VK_CONTROL) && !held(VK_MENU);
    constexpr LPARAM repeated_key_bit = LPARAM{1} << 30;
    switch (message)
    {
    case WM_KEYDOWN:
        send_settings_all(settings_key_intents(
            SettingsKeyPress{word, control, held(VK_SHIFT), (data & repeated_key_bit) != 0},
            view.value(), settings_layout_drawn()));
        return true;
    case WM_CHAR:
        send_settings_all(settings_character_intents(static_cast<wchar_t>(word), control,
                                                     settings_high_surrogate_, view.value()));
        return true;
    default:
        return false;
    }
}

void EditorWindow::send_settings_all(const std::vector<application::EditorIntent> &intents)
{
    for (const auto &intent : intents)
    {
        send_settings(intent);
    }
}

const std::optional<core::SettingsLayout> &EditorWindow::settings_layout_drawn() const
{
    static const std::optional<core::SettingsLayout> none;
    return renderer_ == nullptr ? none : renderer_->settings_layout();
}

// 面が閉じた所で IME の変換を取り消す（deliver の段 cancel_settings_composition・閉じたかは
// delivery_steps が settings_just_closed で決める・レビュー M1・#1854）。IME を開け閉めする
// follow_ime より前なので、Vim の NORMAL へ戻って IME を閉じるときには変換はもう無い（閉じるときに
// IME が確定を本文へ送らない）。取り消しで IME が送り返す通知は、変換の無い本文の道で何もしない。
void EditorWindow::follow_settings_closing()
{
    settings_pressed_ = std::nullopt;
    settings_high_surrogate_ = 0;
    cancel_ime_composition();
}

// 面が開いている間の左の押し。面の中は覚えるだけ（離したときに同じ部品なら動かす）。面の外の押しは
// 閉じて保存するだけで、本文へ渡さない。
bool EditorWindow::press_settings(LPARAM data)
{
    if (!controller_.settings_open())
    {
        return false;
    }
    const auto [pressed, intents] =
        settings_press_step(settings_target_under(low_word_of(data), high_word_of(data)));
    settings_pressed_ = pressed;
    // 閉じるだけだった押しは、続く 2 回目の押しをダブルクリックに数えさせない。
    settings_dismissed_ = !intents.empty();
    send_settings_all(intents);
    return true;
}

bool EditorWindow::release_settings(LPARAM data)
{
    const auto pressed = std::exchange(settings_pressed_, std::nullopt);
    const auto view = controller_.settings_view();
    if (!pressed.has_value() || !view.has_value())
    {
        return false;
    }
    send_settings_all(settings_release_intents(
        pressed, settings_target_under(low_word_of(data), high_word_of(data)), view.value()));
    return true;
}

// 面が開いている間の右の押し。面の外は閉じるだけ、中は何もしない（どちらも本文のメニューを
// 開かない）。
bool EditorWindow::press_settings_right(LPARAM data)
{
    if (!controller_.settings_open())
    {
        return false;
    }
    send_settings_all(
        settings_right_press_intents(settings_target_under(low_word_of(data), high_word_of(data))));
    return true;
}

// 面の外を押して閉じた直後の 2 回目の押しか（読んだら消す）。
bool EditorWindow::take_settings_dismissal() noexcept
{
    return std::exchange(settings_dismissed_, false);
}

bool EditorWindow::over_settings(LPARAM data) const
{
    return controller_.settings_open() &&
           settings_target_under(low_word_of(data), high_word_of(data)).part !=
               core::SettingsPart::outside;
}

core::SettingsTarget EditorWindow::settings_target_under(std::int32_t x, std::int32_t y) const
{
    return renderer_ == nullptr ? core::SettingsTarget{core::SettingsPart::outside, 0}
                                : renderer_->settings_target_at(x, y);
}

// 面の上のホイールは行の領域だけを送る（焦点は動かさない）。面の外は今の道（本文の送り）へ。
bool EditorWindow::wheel_settings(std::int32_t delta, LPARAM data)
{
    if (!controller_.settings_open() || renderer_ == nullptr)
    {
        return false;
    }
    // WM_MOUSEWHEEL の位置は画面の座標なので、クライアントの座標に直してから見る。
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    if (!settings_takes_wheel(renderer_->settings_target_at(point.x, point.y)))
    {
        return false;
    }
    const auto turn = wheel_turned(settings_wheel_remainder_, delta,
                                   wheel_lines_per_notch(wheel_setting_, page_wheel_lines));
    settings_wheel_remainder_ = turn.remainder;
    // 起点は application の状態の送り（描画の前に 2 通来ても 1 通目が消えない・レビュー R2）。
    const auto view = controller_.settings_view();
    const auto top = turn.lines == 0 || !view.has_value()
                         ? std::nullopt
                         : renderer_->settings_scrolled(view.value().scroll, turn.lines);
    if (top.has_value())
    {
        send(application::ScrollSettings{top.value()});
    }
    return true;
}

// 面が開いている間の IME。変換の文字は自前で描かず、OS の変換の窓を探す欄の中に出して、確定を
// WM_CHAR で受ける（探す欄は本文の変換の道を通さない）。面が開いていなければ値なし（今の道へ）。
std::optional<LRESULT> EditorWindow::compose_settings(UINT message, WPARAM word, LPARAM data)
{
    if (!controller_.settings_open())
    {
        return std::nullopt;
    }
    const auto search =
        renderer_ == nullptr ? std::nullopt : renderer_->settings_search_rectangle();
    if (message == WM_IME_STARTCOMPOSITION && search.has_value())
    {
        place_settings_composition(search.value());
    }
    return DefWindowProcW(window_, message, word, data);
}

// OS の変換の窓を探す欄の中（虫めがねの右）に置く。
void EditorWindow::place_settings_composition(const core::LayoutRect &search)
{
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    COMPOSITIONFORM form{};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = POINT{search.left + core::to_pixels(composition_left_dips, dpi_),
                              search.top + core::to_pixels(composition_top_dips, dpi_)};
    ImmSetCompositionWindow(context, &form);
    ImmReleaseContext(window_, context);
}
} // namespace nenenib::ui::win32
