#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "BodyLayout.hpp"
#include "CancelComposition.hpp"
#include "CommitText.hpp"
#include "EditorIntent.hpp"
#include "EditorSettings.hpp"
#include "ExplorerLayout.hpp"
#include "LanguageServerTick.hpp"
#include "MapTimeout.hpp"
#include "PaneLayouts.hpp"
#include "PreviewTick.hpp"
#include "PreviewView.hpp"
#include "SplitLayout.hpp"
#include "TitleBarWidth.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <utility>
#include <variant>

namespace nenenib::ui::win32
{
using detail::font_of;
using detail::product_name;

namespace
{
// 起動の問いの文言（決定 6）。ここの 1 か所だけ。
constexpr wchar_t autosave_question[] = L"前回の未保存の編集が残っています。戻しますか。";
} // namespace

LRESULT EditorWindow::frame_message(UINT message, WPARAM word, LPARAM data)
{
    // 窓の大きさ・DPI・外観の 4 通。
    switch (message)
    {
    case WM_GETMINMAXINFO:
        limit_size(data);
        return 0;
    case WM_SETTINGCHANGE:
        read_wheel_setting();
        refresh_appearance();
        return 0;
    case WM_SIZE:
        resize();
        return 0;
    case WM_DPICHANGED:
        change_dpi(word, data);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

void EditorWindow::refresh_appearance()
{
    // 状態遷移は controller だけが行い、窓は返ってきた表示値を写す（ARC-011）。
    const auto delivery = controller_.apply(application::RefreshAppearance{});
    apply_backdrop(delivery);
    invalidate();
}

void EditorWindow::resize()
{
    if (renderer_ == nullptr)
    {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    // 差し替えの前は子窓を client に合わせてから、そこに結んだ swap chain を合わせる（決定 5）。
    if (cover_ != nullptr)
    {
        cover_->resize(client);
    }
    if (!renderer_->resize(static_cast<UINT>(client.right), static_cast<UINT>(client.bottom)) &&
        !recover())
    {
        abandon();
        return;
    }
    // 何行入るかは application が持つ。窓は寸法から数えた行数を意図として渡すだけ（ADR 0009）。
    send(application::TitleBarWidth{title_bar_width()});
    send(body_view());
}

// 本文の枠の配置（ADR 0148 の決定 8）。窓の行数・マウス・描画が同じ pane_layouts の 1 本を引く。
// エクスプローラーの面が出ていれば、面の幅だけ右へ縮めた client から求める（ADR 0162 の決定 2）。
core::PaneLayouts EditorWindow::body_panes(const core::EditorSettings &settings) const
{
    return core::pane_layouts(explorer_frame(settings).body, dpi_, settings.font_size,
                              controller_.split().layout);
}

// 面の配置。出ているかは設定 explorer（面の表示値が frame に載るのと同じ値）が決める。
core::ExplorerLayout EditorWindow::explorer_frame(const core::EditorSettings &settings) const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::explorer_layout(core::LayoutRect{0, 0, client.right, client.bottom}, dpi_,
                                 settings.font_size, settings.explorer);
}

core::BodyLayout EditorWindow::active_body(const core::EditorSettings &settings) const
{
    return core::active_pane_layout(body_panes(settings), controller_.split().active_side);
}

// 寸法は操作中の枠の本文（両枠の幅と高さは等しいので、もう 1 つの枠も同じ寸法・決定 1）。
application::VisibleLines EditorWindow::body_view() const
{
    const auto delivered = controller_.delivery();
    const auto body = active_body(delivered.settings);
    core::TextViewGeometry geometry{std::max(core::width_of(body.content), 1), body.line_height,
                                    dpi_};
    // 本文の書式の字幅は格子の 1 升と同じ所（BodyTextLayouts::cell）で測る。巨大な行の窓の x の
    // 見積もりに使う（ADR 0139 の決定 3）。測れなければ 0 のまま（application は 1 画素とみなす）。
    const auto advance = text_layouts_.cell(
        application::TextLayoutRequest{{}, delivered.settings, geometry, core::DisplayForm::text});
    geometry.average_advance = advance.has_value() ? advance.value().width : 0.0F;
    // 面と補完の行数は窓全体の寸法から決める（ADR 0150 の決定 1・枠の寸法ではない）。
    RECT client{};
    GetClientRect(window_, &client);
    // 面の行数も窓全体の高さから数える（分割の枠の高さではない・#1125・ADR 0162 の追補）。
    return application::VisibleLines{
        body.visible_lines, geometry, client.right, client.bottom,
        core::explorer_panel_lines(core::LayoutRect{0, 0, client.right, client.bottom}, dpi_,
                                   delivered.settings.font_size)};
}

void EditorWindow::send(const application::EditorIntent &intent)
{
    // close_tab の中の send・MessageBoxW の入れ子のループの中で受けた入力の send は深さ 2 以上。
    ++sending_depth_;
    deliver(intent);
    --sending_depth_;
    // いちばん外の send の終わりで、途中で届いた合図を 1 回だけ送る。close_tab が窓を壊していたら
    // 送らない。送る send も深さを数えるので、その途中で届いた合図はまた次の 1 回にまとまる。
    if (sending_depth_ == 0 && std::exchange(work_waiting_, false) && window_ != nullptr)
    {
        send(application::WorkCompleted{});
    }
}

void EditorWindow::receive_work()
{
    if (sending_depth_ > 0)
    {
        work_waiting_ = true;
        return;
    }
    send(application::WorkCompleted{});
}

void EditorWindow::receive_timer(WPARAM word)
{
    const auto timer = window_timer_of(word);
    if (!timer.has_value())
    {
        return;
    }
    switch (timer.value())
    {
    case WindowTimer::selection:
        scroll_selection();
        return;
    case WindowTimer::autosave:
        send(application::AutosaveTick{});
        return;
    case WindowTimer::preview:
        send(application::PreviewTick{});
        return;
    case WindowTimer::language_server:
        send(application::LanguageServerTick{});
        return;
    case WindowTimer::key_map:
        KillTimer(window_, word);
        send(application::MapTimeout{});
        return;
    }
}

// 残骸の一覧は controller が port から 1 回だけ読む。問いが出せなかった（0）ときは答えを送らず、
// 残骸は次の起動でもう 1 回問われる。
void EditorWindow::offer_autosave()
{
    if (controller_.autosave_remnants().empty())
    {
        return;
    }
    const int answer =
        MessageBoxW(window_, autosave_question, product_name, MB_YESNO | MB_ICONQUESTION);
    if (answer == IDYES)
    {
        send(application::RestoreAutosave{});
        return;
    }
    if (answer == IDNO)
    {
        send(application::DiscardAutosave{});
    }
}

// 字体・字の大きさ・明暗・窓の縁の色は前の配達で当てた値と比べ、変わったときだけ当てる。
// どの意図で変わったか（保存・確定・配色の候補のプレビュー）は見ない（ADR 0150 の決定 5）。
// 字が変われば行の幾何も変わるので、寸法を送り直した delivery を返す。
application::EditorDelivery EditorWindow::follow_look(application::EditorDelivery delivery)
{
    if (delivery.appearance != shown_appearance_ ||
        core::selected_theme(delivery.settings, delivery.appearance).ui.title_bar != shown_border_)
    {
        apply_backdrop(delivery);
    }
    auto font = font_of(delivery.settings);
    if (font == shown_font_)
    {
        return delivery;
    }
    shown_font_ = std::move(font);
    return controller_.apply(body_view());
}

void EditorWindow::deliver(const application::EditorIntent &intent)
{
    const auto previous_layout = controller_.split().layout;
    auto delivery = controller_.apply(intent);
    if (!survives_layout(delivery))
    {
        return;
    }
    // 入力行が閉じて変換が消えたら、IME を開け閉めする前に IME の側の変換も取り消す（ADR 0061
    // の決定 3）。確定と取消は IME が自分で終えたので触らない。取り消しで IME が送り返す通知は
    // 変換の無い delivery に CancelComposition を送るだけ（再入は 1 段で止まる）。
    const bool command_composed = command_composing_;
    command_composing_ = delivery.command_composition.has_value();
    if (command_composed && !command_composing_ &&
        !std::holds_alternative<application::CommitText>(intent) &&
        !std::holds_alternative<application::CancelComposition>(intent))
    {
        cancel_ime_composition();
    }
    // 閉じたいタブは意図を送った結果の delivery にだけ載る（ADR 0057 の決定 6）。下で delivery を
    // 作り直す前に読んでおく。
    const auto close_request = delivery.close_request;
    const bool closing = delivery.closing;
    // 一覧で選んだ操作も同じく意図を送った結果の delivery にだけ載る（ADR 0078 の決定 8）。
    const auto operation_request = delivery.operation_request;
    // 格子表示の問いも同じ（ADR 0140 の決定 2）。
    const bool display_asked = delivery.display_question;
    follow_key_map_timer(window_, delivery.map_timer);
    if (std::holds_alternative<application::AdjustFontSize>(intent))
    {
        announce_settings(delivery);
    }
    delivery = follow_look(std::move(delivery));
    follow_split(previous_layout, delivery);
    follow_explorer(delivery);
    follow_ime(delivery);
    mode_ = delivery.mode;
    vim_mode_ = delivery.vim_mode;
    // 描くのは WM_PAINT。まとめて来た入力はここで無効化だけ積まれ、1 フレームに畳まれる（決定 6）。
    // 何も変えなかった合図は積まない（ADR 0155 の追補・#813）。
    if (delivery.repaint)
    {
        invalidate();
    }
    update_title(delivery);
    announce(delivery);
    finish_operation(operation_request);
    finish_display_question(display_asked);
    finish_tab_action(close_request, closing);
    release_session_block();
    follow_preview_timer();
}

// プレビューの合図は other の枠にプレビューがある間だけ打つ（ADR 0155 の決定 8）。開いているかは
// application の枠の状態が正本で、ui は張ったかどうかだけを覚える（既に張っていれば張り直さない。
// 張り直すと打鍵のたびに数え直しになり、止めてからの tick が遅れる）。
void EditorWindow::follow_preview_timer()
{
    const bool shown = previewing();
    if (shown == preview_ticking_)
    {
        return;
    }
    if (shown)
    {
        preview_ticking_ =
            SetTimer(window_, preview_timer, preview_interval_milliseconds, nullptr) != 0;
        return;
    }
    stop_preview_timer();
}

// other の枠がプレビューか（正本は application の枠の状態）。
bool EditorWindow::previewing() const
{
    const auto &other = controller_.split().other;
    return other.has_value() && std::holds_alternative<application::PreviewView>(other.value());
}

void EditorWindow::stop_preview_timer()
{
    if (preview_ticking_)
    {
        KillTimer(window_, preview_timer);
        preview_ticking_ = false;
    }
}

// 枠を分けた・閉じた（鍵・一覧・Vim の `Ctrl-w`・Ex の `:q`）ら、操作中の枠の行数と幅を
// 送り直す（ADR 0148 の決定 8）。
void EditorWindow::follow_split(core::SplitLayout previous, application::EditorDelivery &delivery)
{
    if (controller_.split().layout != previous)
    {
        delivery = controller_.apply(body_view());
    }
}

// エクスプローラーの面を出した・消した（鍵・一覧・Ex の `set explorer`）ら、本文の幅が変わるので
// 操作中の枠の行数と幅を送り直す（ADR 0162 の決定 2）。面が消えたままの打鍵の道は比べるだけ。
void EditorWindow::follow_explorer(application::EditorDelivery &delivery)
{
    if (delivery.settings.explorer == shown_explorer_)
    {
        return;
    }
    shown_explorer_ = delivery.settings.explorer;
    delivery = controller_.apply(body_view());
}
} // namespace nenenib::ui::win32
