// 設定の面（右の引き出し）の窓の側（施主決定 D102・ADR 0170 の決定 5・6・#1836）。開いている間の
// 鍵と文字は面へ送り、本文へ通さない（通すと application が面を閉じて保存する）。何が変わるか・
// いつ保存するかは controller が決め、窓は意図を送るだけ（ARC-011）。鍵・文字・マウスを意図の列に
// 写すのは SettingsInput の純関数で、ここは値を集めて送るだけ。当たりは renderer が最後に描いた
// 配置（core::settings_layout の 1 本）から引く。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "CloseSettingsChoice.hpp"
#include "DevicePixels.hpp"
#include "EditorIntent.hpp"
#include "JumpSettingsSection.hpp"
#include "LayoutRect.hpp"
#include "OpenSettings.hpp"
#include "PointSettingsChoice.hpp"
#include "ScrollSettings.hpp"
#include "ScrollSettingsChoice.hpp"
#include "SettingsChoices.hpp"
#include "SettingsInput.hpp"
#include "SettingsKeyPress.hpp"
#include "SettingsLayout.hpp"
#include "SettingsPart.hpp"
#include "SettingsTarget.hpp"
#include "TypeSettingsChoice.hpp"
#include "WheelTurn.hpp"
#include "WindowTimer.hpp"

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

// 面の意図を送り、焦点の行が行の領域の外なら見える所へ送りを寄せる。選ぶ欄の一覧が開いていれば、
// 選ぼうとしている行が見える所へ一覧の送りも寄せる（#1926）。一覧へ字を送ったら、打った字を忘れる
// 1 秒のタイマーを張り直す（時刻は読まず OS のタイマーに数えさせる・MapTimeout と同じ形）。
void EditorWindow::send_settings(const application::EditorIntent &intent)
{
    settings_choice_hovered_ = std::nullopt;
    send(intent);
    if (std::holds_alternative<application::TypeSettingsChoice>(intent) && window_ != nullptr)
    {
        static_cast<void>(SetTimer(window_, std::to_underlying(WindowTimer::settings_choice),
                                   settings_choice_typing_milliseconds, nullptr));
    }
    if (renderer_ == nullptr)
    {
        return;
    }
    const auto view = controller_.settings_view();
    if (!view.has_value())
    {
        return;
    }
    if (view.value().choice.has_value())
    {
        const auto shown =
            renderer_->settings_choice_revealed(view.value(), controller_.display_language());
        if (shown.has_value())
        {
            send(application::ScrollSettingsChoice{shown.value()});
        }
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
    case WM_SYSKEYDOWN:
        return take_settings_system_key(word, view.value());
    default:
        return false;
    }
}

// Alt+↓ / Alt+↑ は選ぶ欄の一覧を開く・閉じる（Windows のコンボボックスの標準・#1926）。ほかの Alt
// の 鍵（Alt+F4 など）は受けず、OS の既定処理へ渡す。
bool EditorWindow::take_settings_system_key(WPARAM word, const application::SettingsView &view)
{
    const auto intents = settings_system_key_intents(word, view);
    if (!intents.has_value())
    {
        return false;
    }
    send_settings_all(intents.value());
    return true;
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
// delivery_step_due が settings_just_closed で決める・レビュー M1・#1854）。IME を開け閉めする
// follow_ime より前なので、Vim の NORMAL へ戻って IME を閉じるときには変換はもう無い（閉じるときに
// IME が確定を本文へ送らない）。取り消しで IME が送り返す通知は、変換の無い本文の道で何もしない。
void EditorWindow::follow_settings_closing()
{
    settings_pressed_ = std::nullopt;
    settings_choice_pressed_ = std::nullopt;
    settings_choice_hovered_ = std::nullopt;
    settings_high_surrogate_ = 0;
    cancel_ime_composition();
}

// 面が開いている間の左の押し。面の中は覚えるだけ（離したときに同じ部品なら動かす）。面の外の押しは
// 閉じて保存するだけで、本文へ渡さない。
bool EditorWindow::press_settings(LPARAM data)
{
    const auto view = controller_.settings_view();
    if (!controller_.settings_open() || !view.has_value())
    {
        return false;
    }
    // 選ぶ欄の一覧が開いている間: 行の押しは覚えて載せる、一覧の外の押しは決めずに閉じる
    // だけ（ほかの物を動かさない・#1926）。
    if (view.value().choice.has_value())
    {
        const auto [held, closing] =
            settings_choice_press_step(settings_choice_inside(data), settings_choice_under(data));
        settings_pressed_ = std::nullopt;
        settings_choice_pressed_ = held;
        settings_dismissed_ = !closing.empty() && !held.has_value();
        send_settings_all(closing);
        return true;
    }
    const auto [pressed, intents] = settings_press_step(
        settings_target_under(low_word_of(data), high_word_of(data)), view.value());
    settings_pressed_ = pressed;
    // 閉じるだけだった押しは、続く 2 回目の押しをダブルクリックに数えさせない。
    settings_dismissed_ = !intents.empty();
    send_settings_all(intents);
    return true;
}

bool EditorWindow::release_settings(LPARAM data)
{
    const auto chose = std::exchange(settings_choice_pressed_, std::nullopt);
    if (chose.has_value())
    {
        send_settings_all(settings_choice_release_intents(chose, settings_choice_under(data)));
        return true;
    }
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
    // 一覧が開いていれば、どこの右クリックも一覧を決めずに閉じるだけ（#1926）。
    if (settings_choice_shown())
    {
        send_settings(application::CloseSettingsChoice{});
        return true;
    }
    send_settings_all(
        settings_right_press_intents(settings_target_under(low_word_of(data), high_word_of(data))));
    return true;
}

// 選ぶ欄の一覧を描いているか（描いた配置があるか）。
bool EditorWindow::settings_choice_shown() const noexcept
{
    return renderer_ != nullptr && renderer_->settings_choice_layout().has_value();
}

// 点の下の一覧の選択肢（一覧の外と上下の空きは値なし）。
std::optional<std::size_t> EditorWindow::settings_choice_under(LPARAM data) const
{
    if (renderer_ == nullptr)
    {
        return std::nullopt;
    }
    const auto &layout = renderer_->settings_choice_layout();
    if (!layout.has_value())
    {
        return std::nullopt;
    }
    return core::settings_choice_hit(layout.value(), low_word_of(data), high_word_of(data));
}

bool EditorWindow::settings_choice_inside(LPARAM data) const
{
    if (renderer_ == nullptr)
    {
        return false;
    }
    const auto &layout = renderer_->settings_choice_layout();
    return layout.has_value() &&
           core::contains(layout.value().panel, low_word_of(data), high_word_of(data));
}

// 一覧の上のマウスの動き: 載せた行を選ぼうとしている行にする（同じ行の動きでは送らない）。一覧の上
// なら真（下の面・本文へ渡さない）。
bool EditorWindow::point_settings_choice(LPARAM data)
{
    if (!settings_choice_inside(data))
    {
        return false;
    }
    const auto under = settings_choice_under(data);
    if (under.has_value() && under != settings_choice_hovered_)
    {
        send_settings(application::PointSettingsChoice{under.value()});
        settings_choice_hovered_ = under;
    }
    return true;
}

// 窓の大きさが変わったら一覧を決めずに閉じる（#1926・設計席の決め）。
void EditorWindow::close_settings_choice_on_resize()
{
    if (settings_choice_shown())
    {
        send(application::CloseSettingsChoice{});
    }
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
    if (settings_choice_shown())
    {
        return wheel_settings_choice(delta, point);
    }
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

// 一覧が開いている間のホイール: 一覧の上は一覧を送る（選ぼうとしている位置は動かさない）、面の上の
// ほかの所は何もしない、面の外は今の道（本文の送り・本文の操作なので一覧は閉じる）へ（#1926）。
bool EditorWindow::wheel_settings_choice(std::int32_t delta, POINT point)
{
    const auto &layout = renderer_->settings_choice_layout();
    if (!layout.has_value() || !core::contains(layout.value().panel, point.x, point.y))
    {
        return settings_takes_wheel(renderer_->settings_target_at(point.x, point.y));
    }
    const auto turn =
        wheel_turned(settings_wheel_remainder_, delta,
                     wheel_lines_per_notch(wheel_setting_, core::settings_choice_rows));
    settings_wheel_remainder_ = turn.remainder;
    const auto view = controller_.settings_view();
    const auto top = turn.lines == 0 || !view.has_value()
                         ? std::nullopt
                         : renderer_->settings_choice_scrolled(
                               view.value(), controller_.display_language(), turn.lines);
    if (top.has_value())
    {
        send(application::ScrollSettingsChoice{top.value()});
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
