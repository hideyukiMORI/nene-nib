// 設定の面（右の引き出し）の窓の側（施主決定 D102・ADR 0170 の決定 5・6・#1836）。開いている間の
// 鍵と文字は面へ送り、本文へ通さない（通すと application が面を閉じて保存する）。何が変わるか・
// いつ保存するかは controller が決め、窓は意図を送るだけ（ARC-011）。当たりと送りの量は renderer が
// 最後に描いた配置（core::settings_layout の 1 本）から引く。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "ChangeSetting.hpp"
#include "ChooseSettingsPreset.hpp"
#include "CloseSettings.hpp"
#include "DevicePixels.hpp"
#include "FilterSettings.hpp"
#include "FocusSettingsRow.hpp"
#include "JumpSettingsSection.hpp"
#include "LayoutRect.hpp"
#include "OpenSettings.hpp"
#include "RevertSettings.hpp"
#include "ScrollSettings.hpp"
#include "SettingsControl.hpp"
#include "SettingsKeyAction.hpp"
#include "SettingsKeys.hpp"
#include "SettingsPanel.hpp"
#include "SettingsPart.hpp"
#include "SettingsTarget.hpp"
#include "StepSettings.hpp"
#include "Utf16.hpp"
#include "WheelTurn.hpp"

#include <imm.h>
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
using detail::held;
using detail::high_word_of;
using detail::low_word_of;

namespace
{
constexpr wchar_t first_high_surrogate = 0xD800;
constexpr wchar_t first_low_surrogate = 0xDC00;
constexpr wchar_t last_low_surrogate = 0xDFFF;
constexpr wchar_t first_printable = 0x20;
constexpr wchar_t delete_character = 0x7F;
constexpr unsigned char continuation_mask = 0xC0;
constexpr unsigned char continuation_mark = 0x80;
// 1 画面の設定のホイールの 1 刻みで送る行の数。
constexpr std::size_t page_wheel_lines = 10;
// 変換の窓を探す欄の中に置く位置（虫めがねの右・DIP）。
constexpr std::int32_t composition_left_dips = 34;
constexpr std::int32_t composition_top_dips = 8;

// 最後の 1 文字（UTF-8 のコードポイント）を除いた文字。
[[nodiscard]] std::string without_last_character(std::string text)
{
    while (!text.empty() &&
           (static_cast<unsigned char>(text.back()) & continuation_mask) == continuation_mark)
    {
        text.pop_back();
    }
    if (!text.empty())
    {
        text.pop_back();
    }
    return text;
}
} // namespace

// 操作「設定」（Ctrl+, と F1 の一覧）。開いている間の Ctrl+, は面の鍵が先に受けて閉じる。
void EditorWindow::run_open_settings()
{
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
    const auto top = renderer_->settings_revealed(view.value(), controller_.display_language());
    if (top.has_value())
    {
        send(application::ScrollSettings{top.value()});
    }
}

bool EditorWindow::take_settings_key(UINT message, WPARAM word)
{
    if (!controller_.settings_open())
    {
        return false;
    }
    switch (message)
    {
    case WM_KEYDOWN:
        press_settings_key(word);
        return true;
    case WM_CHAR:
        type_settings_character(word);
        return true;
    default:
        return false;
    }
}

void EditorWindow::press_settings_key(WPARAM word)
{
    const bool control = held(VK_CONTROL) && !held(VK_MENU);
    const auto section = settings_section_key(word, control);
    if (section.has_value())
    {
        send_settings(
            application::JumpSettingsSection{core::settings_sections.at(section.value())});
        return;
    }
    switch (settings_key_action(word, control, held(VK_SHIFT)))
    {
    case SettingsKeyAction::swallow:
        return;
    case SettingsKeyAction::close:
        send(application::CloseSettings{});
        return;
    case SettingsKeyAction::revert:
        send_settings(application::RevertSettings{});
        return;
    case SettingsKeyAction::step_up:
        send_settings(application::StepSettings{false});
        return;
    case SettingsKeyAction::step_down:
        send_settings(application::StepSettings{true});
        return;
    case SettingsKeyAction::page_up:
        page_settings(false);
        return;
    case SettingsKeyAction::page_down:
        page_settings(true);
        return;
    case SettingsKeyAction::first:
        send_settings(application::FocusSettingsRow{0});
        return;
    case SettingsKeyAction::last:
        send_settings(application::FocusSettingsRow{std::numeric_limits<std::size_t>::max()});
        return;
    case SettingsKeyAction::change:
        send_settings(application::ChangeSetting{true});
        return;
    case SettingsKeyAction::previous_value:
        turn_setting(false);
        return;
    case SettingsKeyAction::next_value:
        turn_setting(true);
        return;
    case SettingsKeyAction::erase:
        erase_settings_character();
        return;
    }
    std::unreachable();
}

// PageUp / PageDown。行き先は最後に描いた配置から引く（描く前は 1 行だけ動く）。
void EditorWindow::page_settings(bool down)
{
    const auto view = controller_.settings_view();
    const auto row = renderer_ == nullptr || !view.has_value()
                         ? std::nullopt
                         : renderer_->settings_paged_row(view.value().focused, down);
    if (row.has_value())
    {
        send_settings(application::FocusSettingsRow{row.value()});
        return;
    }
    send_settings(application::StepSettings{down});
}

// ← →。値の行は前後へ送る。スイッチの行は ← でオフ・→ でオン（もうその側なら何もしない）。
void EditorWindow::turn_setting(bool forward)
{
    const auto view = controller_.settings_view();
    if (!view.has_value() || !view.value().focused.has_value())
    {
        return;
    }
    const auto &row = view.value().rows.at(view.value().focused.value());
    const auto control = core::settings_control_of(row.row.subject, row.switchable);
    if (control == core::SettingsControl::toggle && row.on == forward)
    {
        return;
    }
    send_settings(application::ChangeSetting{forward});
}

void EditorWindow::erase_settings_character()
{
    const auto view = controller_.settings_view();
    if (view.has_value() && !view.value().query.empty())
    {
        send_settings(application::FilterSettings{without_last_character(view.value().query)});
    }
}

// 文字（IME の確定を含む）は探す欄へ。探す欄が空のときの Space は焦点の行の切り替え。
void EditorWindow::type_settings_character(WPARAM word)
{
    const auto unit = static_cast<wchar_t>(word);
    if (unit >= first_high_surrogate && unit < first_low_surrogate)
    {
        settings_high_surrogate_ = unit;
        return;
    }
    const wchar_t pending = std::exchange(settings_high_surrogate_, 0);
    const bool low = unit >= first_low_surrogate && unit <= last_low_surrogate;
    // 制御文字（Ctrl つきの鍵・Enter・Tab・Backspace）は WM_KEYDOWN が扱う。
    if (unit < first_printable || unit == delete_character || (low && pending == 0))
    {
        return;
    }
    const auto view = controller_.settings_view();
    if (!view.has_value())
    {
        return;
    }
    if (unit == first_printable && view.value().query.empty())
    {
        send_settings(application::ChangeSetting{true});
        return;
    }
    std::wstring wide;
    if (low)
    {
        wide.push_back(pending);
    }
    wide.push_back(unit);
    send_settings(application::FilterSettings{view.value().query +
                                              core::to_utf8(wide).value_or(std::string{})});
}

// 面が開いている間の左の押し。面の中は覚えるだけ（離したときに同じ部品なら動かす）。面の外の押しは
// 閉じて保存するだけで、本文へ渡さない。
bool EditorWindow::press_settings(LPARAM data)
{
    if (!controller_.settings_open())
    {
        return false;
    }
    const auto target = settings_target_under(low_word_of(data), high_word_of(data));
    if (target.part == core::SettingsPart::outside)
    {
        settings_pressed_ = std::nullopt;
        send(application::CloseSettings{});
        return true;
    }
    settings_pressed_ = target;
    return true;
}

bool EditorWindow::release_settings(LPARAM data)
{
    const auto pressed = std::exchange(settings_pressed_, std::nullopt);
    if (!pressed.has_value() || !controller_.settings_open())
    {
        return false;
    }
    // 押した部品と離した部品が同じときだけ動かす（ADR 0056 の決定 9 と同じ考え）。
    if (settings_target_under(low_word_of(data), high_word_of(data)) == pressed.value())
    {
        act_on_settings(pressed.value());
    }
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
    if (settings_target_under(low_word_of(data), high_word_of(data)).part ==
        core::SettingsPart::outside)
    {
        send(application::CloseSettings{});
    }
    return true;
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

void EditorWindow::act_on_settings(core::SettingsTarget target)
{
    switch (target.part)
    {
    case core::SettingsPart::outside:
    case core::SettingsPart::panel:
    case core::SettingsPart::search:
        return;
    case core::SettingsPart::close:
    case core::SettingsPart::close_button:
        send(application::CloseSettings{});
        return;
    case core::SettingsPart::chip:
        send_settings(
            application::JumpSettingsSection{core::settings_sections.at(target.index)});
        return;
    case core::SettingsPart::preset:
        send_settings(
            application::ChooseSettingsPreset{core::settings_presets.at(target.index)});
        return;
    case core::SettingsPart::revert:
        send_settings(application::RevertSettings{});
        return;
    case core::SettingsPart::row:
    case core::SettingsPart::row_value:
    case core::SettingsPart::row_minus:
    case core::SettingsPart::row_plus:
        click_settings_row(target);
        return;
    }
    std::unreachable();
}

// 行のクリックは焦点を置き、スイッチの行はどこを押しても切り替える。選ぶ欄は欄を押すと次の値、
// 段の上下は − と ＋。
void EditorWindow::click_settings_row(core::SettingsTarget target)
{
    const bool toggle = renderer_ != nullptr && renderer_->settings_row_control(target.index) ==
                                                    std::optional{core::SettingsControl::toggle};
    send_settings(application::FocusSettingsRow{target.index});
    if (target.part == core::SettingsPart::row && !toggle)
    {
        return;
    }
    send_settings(application::ChangeSetting{target.part != core::SettingsPart::row_minus});
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
    if (renderer_->settings_target_at(point.x, point.y).part == core::SettingsPart::outside)
    {
        return false;
    }
    const auto turn = wheel_turned(settings_wheel_remainder_, delta,
                                   wheel_lines_per_notch(wheel_setting_, page_wheel_lines));
    settings_wheel_remainder_ = turn.remainder;
    const auto top = turn.lines == 0 ? std::nullopt : renderer_->settings_scrolled(turn.lines);
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
