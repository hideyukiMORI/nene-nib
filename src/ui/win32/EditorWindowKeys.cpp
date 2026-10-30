#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "BookmarkKey.hpp"
#include "CaretMotion.hpp"
#include "EditMode.hpp"
#include "EditorIntent.hpp"
#include "KeyChord.hpp"
#include "KeyMotion.hpp"
#include "KeyVimSpecial.hpp"
#include "Milestone.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenCommandPalette.hpp"
#include "OperationBindings.hpp"
#include "OperationShortcut.hpp"
#include "SearchHop.hpp"
#include "SelectionAnchoring.hpp"
#include "SettleRecentTab.hpp"
#include "TabCommand.hpp"
#include "TabKeyTable.hpp"
#include "TabShortcut.hpp"
#include "TabStep.hpp"
#include "ToggleBookmark.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSearchDirection.hpp"
#include "VimSpecialKey.hpp"
#include "WalkRecentTab.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
using detail::held;

namespace
{
constexpr wchar_t first_high_surrogate = 0xD800;
constexpr wchar_t first_low_surrogate = 0xDC00;
constexpr wchar_t last_low_surrogate = 0xDFFF;
constexpr wchar_t first_printable = 0x20;
constexpr wchar_t delete_character = 0x7F;

// キー → キャレットの移動は表で引く（CPP-012）。Ctrl の有無は表そのものを分けて表す。
constexpr std::array<KeyMotion, 8> plain_motions{{{VK_LEFT, core::CaretMotion::previous_character},
                                                  {VK_RIGHT, core::CaretMotion::next_character},
                                                  {VK_UP, core::CaretMotion::previous_line},
                                                  {VK_DOWN, core::CaretMotion::next_line},
                                                  {VK_HOME, core::CaretMotion::line_start},
                                                  {VK_END, core::CaretMotion::line_end},
                                                  {VK_PRIOR, core::CaretMotion::page_up},
                                                  {VK_NEXT, core::CaretMotion::page_down}}};
constexpr std::array<KeyMotion, 4> control_motions{{{VK_LEFT, core::CaretMotion::previous_word},
                                                    {VK_RIGHT, core::CaretMotion::next_word},
                                                    {VK_HOME, core::CaretMotion::document_start},
                                                    {VK_END, core::CaretMotion::document_end}}};
// Vim モードの仮想キー → 特別な鍵。通常モードの表と入れ替えて引く（ADR 0012 の決定 4）。
constexpr std::array<KeyVimSpecial, 11> vim_specials{{{VK_ESCAPE, core::VimSpecialKey::escape},
                                                      {VK_RETURN, core::VimSpecialKey::enter},
                                                      {VK_BACK, core::VimSpecialKey::backspace},
                                                      {VK_LEFT, core::VimSpecialKey::arrow_left},
                                                      {VK_RIGHT, core::VimSpecialKey::arrow_right},
                                                      {VK_UP, core::VimSpecialKey::arrow_up},
                                                      {VK_DOWN, core::VimSpecialKey::arrow_down},
                                                      {VK_HOME, core::VimSpecialKey::home},
                                                      {VK_END, core::VimSpecialKey::end},
                                                      {VK_PRIOR, core::VimSpecialKey::page_up},
                                                      {VK_NEXT, core::VimSpecialKey::page_down}}};
// Ctrl+W は Vim の窓の命令の入口（ADR 0148 の決定 7）。INSERT でも engine へ流し、engine
// が語を消す（#1040）。Ctrl+O はジャンプの戻り（#1037・D71）で、INSERT では engine が無視する。
constexpr std::array<KeyVimSpecial, 6> vim_control_specials{
    {{'D', core::VimSpecialKey::control_d},
     {'U', core::VimSpecialKey::control_u},
     {'F', core::VimSpecialKey::control_f},
     {'B', core::VimSpecialKey::control_b},
     {'W', core::VimSpecialKey::control_w},
     {'O', core::VimSpecialKey::control_o}}};
constexpr char32_t tab_character = U'\t';

// いま押されている修飾と仮想キー → 操作の鍵。Alt と組んだ鍵は写さない（ADR 0078 の決定 2）。
[[nodiscard]] std::optional<core::KeyChord> held_chord(WPARAM word)
{
    if (held(VK_MENU))
    {
        return std::nullopt;
    }
    return operation_chord(word, held(VK_CONTROL), held(VK_SHIFT));
}

[[nodiscard]] std::optional<core::CaretMotion> motion_for(std::span<const KeyMotion> table,
                                                          WPARAM key) noexcept
{
    for (const KeyMotion entry : table)
    {
        if (entry.key == key)
        {
            return entry.motion;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<core::VimSpecialKey> vim_special_for(WPARAM key) noexcept
{
    for (const KeyVimSpecial entry : vim_specials)
    {
        if (entry.key == key)
        {
            return entry.special;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<core::VimSpecialKey> vim_control_special_for(WPARAM key) noexcept
{
    for (const KeyVimSpecial entry : vim_control_specials)
    {
        if (entry.key == key)
        {
            return entry.special;
        }
    }
    return std::nullopt;
}

[[nodiscard]] core::SelectionAnchoring anchoring_now() noexcept
{
    return held(VK_SHIFT) ? core::SelectionAnchoring::extend : core::SelectionAnchoring::collapse;
}

// Ctrl+V を矩形の鍵として送るモード（ADR 0035 の決定 9）。INSERT だけは OS の貼付に譲る。
[[nodiscard]] bool vim_block_key(core::VimMode vim) noexcept
{
    switch (vim)
    {
    case core::VimMode::insert:
        return false;
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return true;
    }
    std::unreachable();
}

// Vim モードで Ctrl と一緒に押された鍵（ADR 0019 / ADR 0035 の決定 9）。表に無い鍵と、
// INSERT の Ctrl+V（OS の貼付）・Ctrl+A・Ctrl+X は空を返し、呼ぶ側の通常モードの表へ落ちる。
// Ctrl+A / Ctrl+X は NORMAL と VISUAL で数の増減（ADR 0015 の決定 14）。
[[nodiscard]] std::optional<core::VimSpecialKey> vim_control_key(WPARAM word,
                                                                 core::VimMode vim) noexcept
{
    constexpr std::array<KeyVimSpecial, 3> command_keys{{{'V', core::VimSpecialKey::control_v},
                                                         {'A', core::VimSpecialKey::control_a},
                                                         {'X', core::VimSpecialKey::control_x}}};
    const auto special = vim_control_special_for(word);
    if (special.has_value() || !vim_block_key(vim))
    {
        return special;
    }
    const auto command = std::ranges::find(command_keys, word, &KeyVimSpecial::key);
    return command == command_keys.end() ? std::nullopt : std::optional{command->special};
}
} // namespace

LRESULT EditorWindow::key_message(UINT message, WPARAM word, LPARAM data)
{
    // 鍵の 3 通とフォーカスを失ったとき。Ctrl を離したとき・フォーカスを失ったときは、歩いて
    // いれば使った順を確定してから OS の既定処理へ渡す（ADR 0058 の決定 4）。
    switch (message)
    {
    case WM_KEYDOWN:
        end_selection_drag();
        timing_.mark(core::Milestone::input_received);
        if (!press_bookmark_key(word, data) && !press_function_key(word, data) &&
            !press_mode_key(word, data))
        {
            press_key(word);
        }
        return 0;
    case WM_CHAR:
        end_selection_drag();
        timing_.mark(core::Milestone::input_received);
        type_character(word);
        return 0;
    case WM_KEYUP:
        if (word == VK_CONTROL)
        {
            settle_tab_walk();
            // Ctrl を離したら字の大きさのホイールの残りを捨てる（ADR 0150 の追補・#1048）。
            zoom_wheel_remainder_ = 0;
        }
        break;
    case WM_KILLFOCUS:
        end_selection_drag();
        settle_tab_walk();
        // よそで Ctrl を離すと WM_KEYUP は来ないので、ここでも残りを捨てる（#1048）。
        zoom_wheel_remainder_ = 0;
        break;
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

// 歩いていないときは意図を送らない（Ctrl+C などのたびに描き直さない・決定 4）。
void EditorWindow::settle_tab_walk()
{
    if (controller_.tab_walking())
    {
        send(application::SettleRecentTab{});
    }
}

void EditorWindow::type_character(WPARAM word)
{
    // 面に焦点がある間の文字は面の鍵（WM_KEYDOWN の表）だけで、本文へ入れない（ADR 0162 の決定
    // 4）。
    if ((held(VK_CONTROL) && !held(VK_MENU)) || controller_.explorer_focused())
    {
        return;
    }
    const auto unit = static_cast<wchar_t>(word);
    if (unit >= first_high_surrogate && unit < first_low_surrogate)
    {
        pending_high_surrogate_ = unit;
        return;
    }
    const wchar_t pending = std::exchange(pending_high_surrogate_, 0);
    const bool low = unit >= first_low_surrogate && unit <= last_low_surrogate;
    // 制御文字は WM_KEYDOWN が扱う。対になっていない下位サロゲートと一緒にここで捨てる。
    if (unit < first_printable || unit == delete_character || (low && pending == 0))
    {
        return;
    }
    std::wstring wide;
    if (low)
    {
        wide.push_back(pending);
    }
    wide.push_back(unit);
    // 合成した 1 文字を UTF-8 の意図へ（CPP-014 / ADR 0009 の決定 2）。変換は core::to_utf8
    // ただ 1 本で、対にならないサロゲートはここまでに捨ててあるので空にはならない（Issue #13）。
    type_text(core::to_utf8(wide).value_or(std::string{}));
}

void EditorWindow::type_text(std::string utf8)
{
    if (controller_.command_line_active())
    {
        send(application::CommandText{std::move(utf8)});
        return;
    }
    switch (mode_)
    {
    case core::EditMode::vim:
        send(application::VimKeyPress{
            core::VimKey{core::VimCharacter{core::code_point_at(utf8, core::Offset{0})}}});
        return;
    case core::EditMode::ordinary:
        break;
    }
    send(application::InsertText{std::move(utf8)});
}

// Ctrl+Dの意味はcoreの表の1本。長押しの反復は登録を付け外ししない（D36・ADR 0063）。
bool EditorWindow::press_bookmark_key(WPARAM word, LPARAM data)
{
    if (word != 'D' || !held(VK_CONTROL) || held(VK_MENU))
    {
        return false;
    }
    const auto key =
        held(VK_SHIFT) ? core::BookmarkKey::control_shift_d : core::BookmarkKey::control_d;
    if (!core::toggles_bookmark(key, mode_))
    {
        return false;
    }
    constexpr LPARAM repeated_key_bit = LPARAM{1} << 30;
    if ((data & repeated_key_bit) == 0 && !controller_.is_composing())
    {
        send(application::ToggleBookmark{});
    }
    return true;
}

// F1 は入力行や面が開いていても効く（ADR 0078 の決定 9）。F5 の日時の挿入も同じで、入力行へ
// 入れるかは controller が決める（ADR 0175 の決定 3）。長押しの反復では送らない（ブックマークの
// 鍵と同じ守り）。変換中の守りは controller にある。
bool EditorWindow::press_function_key(WPARAM word, LPARAM data)
{
    const auto chord = held_chord(word);
    if (!chord.has_value() || (chord.value().key != core::OperationKey::f1 &&
                               chord.value().key != core::OperationKey::f5))
    {
        return false;
    }
    const auto operation = core::operation_for(chord.value(), mode_);
    if (!operation.has_value())
    {
        return false;
    }
    constexpr LPARAM repeated_key_bit = LPARAM{1} << 30;
    if ((data & repeated_key_bit) == 0)
    {
        run_operation(operation.value());
    }
    return true;
}

// 通常 / Vim の切り替えの鍵（Ctrl+Shift+Space・D61）は押した最初の 1 回だけ。長押しの反復では
// 切り替えない（ブックマークの鍵と同じ守り）。入力行が開いていれば入力行の鍵のまま（press_key）。
bool EditorWindow::press_mode_key(WPARAM word, LPARAM data)
{
    if (controller_.command_line_active())
    {
        return false;
    }
    const auto chord = held_chord(word);
    const auto operation =
        chord.has_value() ? core::operation_for(chord.value(), mode_) : std::nullopt;
    if (operation != core::EditorOperation::toggle_mode)
    {
        return false;
    }
    constexpr LPARAM repeated_key_bit = LPARAM{1} << 30;
    if ((data & repeated_key_bit) == 0)
    {
        run_operation(core::EditorOperation::toggle_mode);
    }
    return true;
}

void EditorWindow::press_key(WPARAM word)
{
    if (controller_.command_line_active())
    {
        press_command_key(word);
        return;
    }
    if (press_tab_key(word))
    {
        return;
    }
    // 操作の鍵は core の表の 1 本で引き、run_operation の 1 本で実行する（ADR 0078 の決定 13）。
    const auto chord = held_chord(word);
    const auto operation =
        chord.has_value() ? core::operation_for(chord.value(), mode_) : std::nullopt;
    if (operation.has_value() && !keeps_block_key(operation.value()))
    {
        run_operation(operation.value());
        return;
    }
    // 面に焦点があれば、操作の鍵のほかは面の鍵の表だけを通す（Vim の表と `map` の表を通さない・
    // ADR 0162 の決定 4）。
    if (controller_.explorer_focused())
    {
        press_explorer_key(word);
        return;
    }
    if (held(VK_CONTROL))
    {
        press_control_key(word);
        return;
    }
    switch (mode_)
    {
    case core::EditMode::vim:
        press_vim_key(word);
        return;
    case core::EditMode::ordinary:
        break;
    }
    press_plain_key(word);
}

// Vim の NORMAL / VISUAL の Ctrl+Shift+V は今までどおり矩形の入口（ADR 0035 の決定 9）。表の
// paste_plain（ADR 0157 の決定 2）より先に、Vim の Ctrl の表へ流す。
bool EditorWindow::keeps_block_key(core::EditorOperation operation) const noexcept
{
    return operation == core::EditorOperation::paste_plain && mode_ == core::EditMode::vim &&
           vim_block_key(vim_mode_);
}

// タブの鍵は Vim の Ctrl の表より先に引く。扱ったら true。値なしの鍵（Vim の Ctrl+W）は false で
// 後ろの表へ流し、Vim の Ctrl の表が engine の `Ctrl-w` に写す（ADR 0148 の決定 7）。IME の変換中も
// 効く（切り替えが変換中を捨てる・ADR 0056 の決定 4・10）。
bool EditorWindow::press_tab_key(WPARAM word)
{
    if (!held(VK_CONTROL) || held(VK_MENU))
    {
        return false;
    }
    const auto key = tab_shortcut(word, held(VK_SHIFT));
    if (!key.has_value())
    {
        return false;
    }
    const auto command = core::tab_command_for(key.value(), mode_);
    if (!command.has_value())
    {
        return false;
    }
    run_tab_command(command.value());
    return true;
}

void EditorWindow::run_tab_command(core::TabCommand command)
{
    switch (command)
    {
    case core::TabCommand::open:
        send(application::NewTab{});
        return;
    case core::TabCommand::next:
        send(application::WalkRecentTab{core::TabStep::next});
        return;
    case core::TabCommand::previous:
        send(application::WalkRecentTab{core::TabStep::previous});
        return;
    case core::TabCommand::close:
    {
        // 閉じるのはアクティブのタブ。位置は状態から読む（表示値を作らない・決定 9）。
        RECT client{};
        GetClientRect(window_, &client);
        close_tab(controller_.title_bar_input(client.right, dpi_).active);
        return;
    }
    }
    std::unreachable();
}

void EditorWindow::press_command_control_key(WPARAM word)
{
    switch (word)
    {
    case 'V':
        send(application::PasteCommand{});
        return;
    case 'C':
        send(application::CancelCommand{});
        return;
    // 面が開いていれば Ctrl+P は前の候補（ADR 0150 の決定 2）。`:` と検索の行では面を開く。
    case 'P':
        send_palette_or(core::CommandEdit::complete_previous, application::OpenCommandPalette{});
        return;
    case 'N':
        send_palette_or(core::CommandEdit::complete_next, std::nullopt);
        return;
    case VK_HOME:
        send_palette_or(core::CommandEdit::list_first, std::nullopt);
        return;
    case VK_END:
        send_palette_or(core::CommandEdit::list_last, std::nullopt);
        return;
    // incsearch の次・前の当たり（ADR 0043 の決定 4）。検索の入力行でなければ controller が
    // 何もしない（Ex と設定一覧は今までどおり素通り）。
    case 'G':
        send(application::SearchHop{core::VimSearchDirection::forward});
        return;
    case 'T':
        send(application::SearchHop{core::VimSearchDirection::backward});
        return;
    default:
        break;
    }
}

void EditorWindow::send_palette_or(core::CommandEdit edit,
                                   const std::optional<application::EditorIntent> &otherwise)
{
    if (controller_.command_palette_active())
    {
        send(application::EditCommand{edit});
        return;
    }
    if (otherwise.has_value())
    {
        send(otherwise.value());
    }
}

// 面だけの鍵（ADR 0150 の決定 2）。上下は前・次（回り込む）、PgUp / PgDn は 1 画面（端で止まる）。
// `:` と検索の行では素通りして下の表へ（PgUp / PgDn は何もしない）。
bool EditorWindow::press_palette_key(WPARAM word)
{
    if (!controller_.command_palette_active())
    {
        return false;
    }
    switch (word)
    {
    case VK_UP:
        send(application::EditCommand{core::CommandEdit::complete_previous});
        return true;
    case VK_DOWN:
        send(application::EditCommand{core::CommandEdit::complete_next});
        return true;
    case VK_PRIOR:
        send(application::EditCommand{core::CommandEdit::page_previous});
        return true;
    case VK_NEXT:
        send(application::EditCommand{core::CommandEdit::page_next});
        return true;
    default:
        return false;
    }
}

void EditorWindow::press_command_key(WPARAM word)
{
    if (held(VK_CONTROL) && !held(VK_MENU))
    {
        press_command_control_key(word);
        return;
    }
    if (press_palette_key(word))
    {
        return;
    }
    switch (word)
    {
    case VK_RETURN:
        send(application::SubmitCommand{});
        return;
    case VK_ESCAPE:
        send(application::CancelCommand{});
        return;
    case VK_LEFT:
        send(application::EditCommand{core::CommandEdit::left});
        return;
    case VK_RIGHT:
        send(application::EditCommand{core::CommandEdit::right});
        return;
    case VK_HOME:
        send(application::EditCommand{core::CommandEdit::home});
        return;
    case VK_END:
        send(application::EditCommand{core::CommandEdit::end});
        return;
    case VK_BACK:
        send(application::EditCommand{core::CommandEdit::backspace});
        return;
    case VK_DELETE:
        send(application::EditCommand{core::CommandEdit::erase});
        return;
    case VK_TAB:
        send(application::EditCommand{held(VK_SHIFT) ? core::CommandEdit::complete_previous
                                                     : core::CommandEdit::complete_next});
        return;
    // `:` と検索の行の上下は履歴（#725）。面の上下は上の press_palette_key が先に受ける。
    case VK_UP:
        send(application::EditCommand{core::CommandEdit::history_previous});
        return;
    case VK_DOWN:
        send(application::EditCommand{core::CommandEdit::history_next});
        return;
    default:
        break;
    }
}

// Vim モードの窓は鍵を写すだけで、何が起きるかは知らない（ARC-011 / ADR 0012 の決定 4）。
// Tab は INSERT の水平タブとして文字の鍵で渡す（WM_CHAR の 0x09 は制御文字として捨てられる）。
void EditorWindow::press_vim_key(WPARAM word)
{
    if (word == VK_TAB)
    {
        send(application::VimKeyPress{core::VimKey{core::VimCharacter{tab_character}}});
        return;
    }
    const auto special = vim_special_for(word);
    if (special)
    {
        send(application::VimKeyPress{core::VimKey{special.value()}});
    }
}

void EditorWindow::press_plain_key(WPARAM word)
{
    const auto motion = motion_for(std::span<const KeyMotion>(plain_motions), word);
    if (motion)
    {
        send(application::MoveCaret{motion.value(), anchoring_now()});
        return;
    }
    // OS の仮想キーは開いた集合なので、既定分岐を書いてよい唯一の場所（CPP-017）。
    switch (word)
    {
    case VK_BACK:
        send(application::DeleteText{core::DeleteDirection::backward});
        return;
    case VK_DELETE:
        send(application::DeleteText{core::DeleteDirection::forward});
        return;
    case VK_RETURN:
        send(application::NewLine{});
        return;
    case VK_TAB:
        send(application::InsertText{std::string("\t")});
        return;
    case VK_ESCAPE:
        // Esc は窓を閉じない。通常モードでは選択を解くだけ（Issue #7）。
        send(application::CancelSelection{});
        return;
    default:
        break;
    }
}

void EditorWindow::press_control_key(WPARAM word)
{
    if (mode_ == core::EditMode::vim)
    {
        const auto special = vim_control_key(word, vim_mode_);
        if (special.has_value())
        {
            send(application::VimKeyPress{core::VimKey{special.value()}});
            return;
        }
    }
    const auto motion = motion_for(std::span<const KeyMotion>(control_motions), word);
    if (motion)
    {
        send(application::MoveCaret{motion.value(), anchoring_now()});
        return;
    }
    switch (word)
    {
    case 'A':
        send(application::SelectAll{});
        return;
    case 'C':
        send(application::ClipboardAction{application::ClipboardOperation::copy});
        return;
    case 'X':
        send(application::ClipboardAction{application::ClipboardOperation::cut});
        return;
    case 'V':
        send(application::ClipboardAction{application::ClipboardOperation::paste});
        return;
    case 'R':
        send_vim_redo();
        return;
    default:
        break;
    }
}

// 鍵（Ctrl+Tab）は Ctrl を離したときに確定する。一覧から（Ctrl を押していない）は 1 歩で確定する
// （ADR 0078 の決定 10）。
void EditorWindow::walk_recent_tab(core::TabCommand command)
{
    run_tab_command(command);
    if (!held(VK_CONTROL))
    {
        send(application::SettleRecentTab{});
    }
}

void EditorWindow::send_history(core::HistoryDirection direction)
{
    switch (mode_)
    {
    case core::EditMode::vim:
        // Vim では u と Ctrl-r が履歴を動かす。Ctrl+Z / Ctrl+Y はそちらに譲る。
        return;
    case core::EditMode::ordinary:
        break;
    }
    send(application::HistoryAction{direction});
}

void EditorWindow::send_vim_redo()
{
    switch (mode_)
    {
    case core::EditMode::ordinary:
        return;
    case core::EditMode::vim:
        break;
    }
    send(application::VimKeyPress{core::VimKey{core::VimSpecialKey::control_r}});
}
} // namespace nenenib::ui::win32
