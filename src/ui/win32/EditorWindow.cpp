#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "BodyLayout.hpp"
#include "CancelComposition.hpp"
#include "CommitText.hpp"
#include "DeliveryFacts.hpp"
#include "DeliveryStep.hpp"
#include "DeliverySteps.hpp"
#include "EditorIntent.hpp"
#include "EditorSettings.hpp"
#include "ExplorerLayout.hpp"
#include "LanguageServerTick.hpp"
#include "MapTimeout.hpp"
#include "MinimapLayout.hpp"
#include "PaneLayouts.hpp"
#include "PreviewTick.hpp"
#include "SplitLayout.hpp"
#include "TabTipTick.hpp"
#include "TitleBarWidth.hpp"
#include "UiMessage.hpp"
#include "WideUiText.hpp"
#include "WindowLines.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::ui::win32
{
using detail::font_of;
using detail::product_name;

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
        read_hover_setting();
        read_animation_setting();
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
// ミニマップが出ていれば、さらに右端を縮める（ADR 0171 の決定 2・出ていなければ minimap_layout
// は最初の 1 行で返る）。
core::PaneLayouts EditorWindow::body_panes(core::FontSize font_size) const
{
    return core::pane_layouts(pane_client(), dpi_, font_size, controller_.pane_split());
}

core::LayoutRect EditorWindow::pane_client() const
{
    return core::minimap_layout(explorer_frame().body, dpi_, controller_.font_size(),
                                controller_.minimap_shown())
        .body;
}

// 面の配置。出ているかと幅は controller の explorer_width（面の表示値が frame に載るのと同じ値・
// ドラッグの間はその幅・ADR 0162 の追補 D78）が決める。寸法は字の大きさに依らない（#1390）ので、
// マウスの道も delivery を作らない（Issue #1380 の R-2）。
core::ExplorerLayout EditorWindow::explorer_frame() const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::explorer_layout(core::LayoutRect{0, 0, client.right, client.bottom}, dpi_,
                                 controller_.explorer_width());
}

core::BodyLayout EditorWindow::active_body(const core::EditorSettings &settings) const
{
    return core::active_pane_layout(body_panes(settings.font_size),
                                    controller_.split().active_side);
}

// 寸法は操作中の枠の本文。2 枠なら境界の寸法ともう 1 つの枠の寸法と行数も添える（両枠の大きさは
// 違ってよい・ADR 0148 の 2026-11-22 の追補の決定 4・5）。
application::VisibleLines EditorWindow::body_view() const
{
    const auto delivered = controller_.delivery();
    const auto panes = body_panes(delivered.settings.font_size);
    const auto body = core::active_pane_layout(panes, controller_.split().active_side);
    core::TextViewGeometry geometry{std::max(core::width_of(body.content), 1), body.line_height,
                                    dpi_};
    // 本文の書式の字幅は格子の 1 升と同じ所（BodyTextLayouts::cell）で測る。巨大な行の窓の x の
    // 見積もりに使う（ADR 0139 の決定 3）。測れなければ 0 のまま（application は 1 画素とみなす）。
    const auto advance = text_layouts_.cell(
        application::TextLayoutRequest{{}, delivered.settings, geometry, core::DisplayForm::text});
    geometry.average_advance = advance.has_value() ? advance.value().width : 0.0F;
    const auto other = core::other_pane_layout(panes, controller_.split().active_side);
    // 面と補完の行数は窓全体の寸法から決める（ADR 0150 の決定 1・枠の寸法ではない）。
    RECT client{};
    GetClientRect(window_, &client);
    // 面の行数も窓全体の高さから数える（分割の枠の高さではない・#1125・ADR 0162 の追補）。
    auto sent = window_lines(body, core::LayoutRect{0, 0, client.right, client.bottom}, dpi_);
    sent.geometry = geometry;
    if (other.has_value())
    {
        sent.split = core::split_extent(pane_client(), dpi_, delivered.settings.font_size,
                                        controller_.pane_split());
        sent.other_geometry =
            core::TextViewGeometry{std::max(core::width_of(other.value().content), 1),
                                   other.value().line_height, dpi_, geometry.average_advance};
        sent.other_lines = std::max<std::size_t>(other.value().visible_lines, 1);
    }
    return sent;
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
    case WindowTimer::tab_tip:
        KillTimer(window_, word);
        send(application::TabTipTick{GetActiveWindow() == window_});
        return;
    case WindowTimer::caret_motion:
        send_caret_motion();
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
    // 起動の問いの文言（決定 6）は表の 1 行。
    const std::wstring question = wide_ui_text(core::UiMessage::dialog_autosave_restore_question,
                                               controller_.display_language());
    const int answer =
        MessageBoxW(window_, question.c_str(), product_name, MB_YESNO | MB_ICONQUESTION);
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
    const auto previous_layout = pane_arrangement();
    auto delivery = controller_.apply(intent);
    if (!survives_layout(delivery))
    {
        return;
    }
    // 後を追う段（変換の取り消し 2 つ → タイマー → 外観 → 枠 → IME の開け閉め）の順は
    // delivery_order、要るかは delivery_step_due が決める（#1854）。
    // 入力行の変換の取り消しで IME が送り返す通知は
    // 変換の無い delivery に CancelComposition を送るだけ（再入は 1 段で止まる）。
    const DeliveryFacts facts{
        .command_composed =
            std::exchange(command_composing_, delivery.command_composition.has_value()),
        .command_composing = command_composing_,
        .composition_settled = std::holds_alternative<application::CommitText>(intent) ||
                               std::holds_alternative<application::CancelComposition>(intent),
        .settings_was_open = std::exchange(settings_shown_, controller_.settings_open()),
        .settings_open = settings_shown_,
        .font_size_adjusted = std::holds_alternative<application::AdjustFontSize>(intent)};
    // 閉じたいタブは意図を送った結果の delivery にだけ載る（ADR 0057 の決定 6）。段の follow_look
    // が delivery を作り直す前に読んでおく。
    const auto close_request = delivery.close_request;
    const bool closing = delivery.closing;
    // 一覧で選んだ操作も同じく意図を送った結果の delivery にだけ載る（ADR 0078 の決定 8）。
    const auto operation_request = delivery.operation_request;
    // タブのメニューのまとめて閉じる頼みも同じ（ADR 0078 の追補 #1790）。
    const auto tab_sweep = delivery.tab_sweep;
    // 格子表示の問いも同じ（ADR 0140 の決定 2）。
    const bool display_asked = delivery.display_question;
    run_delivery_steps(facts, [this, previous_layout, &delivery](DeliveryStep step)
                       { delivery_step(step, previous_layout, delivery); });
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
    finish_tab_sweep(tab_sweep);
    release_session_block();
    follow_preview_timer();
    // 起動画面が消えたら押しの記憶を消す（押している間だけ読む・ADR 0172 の追補 24）。
    if (start_pressed_.has_value() && !controller_.start_screen().has_value())
    {
        start_pressed_ = std::nullopt;
    }
}

// 後を追う 1 段を写す（順は delivery_order・要るかは delivery_step_due・#1854）。
EditorWindow::PaneArrangement EditorWindow::pane_arrangement() const noexcept
{
    return {controller_.pane_split(), controller_.split().active_side};
}

void EditorWindow::delivery_step(DeliveryStep step, PaneArrangement previous_layout,
                                 application::EditorDelivery &delivery)
{
    switch (step)
    {
    case DeliveryStep::cancel_command_composition:
        cancel_ime_composition();
        return;
    case DeliveryStep::cancel_settings_composition:
        follow_settings_closing();
        return;
    case DeliveryStep::follow_key_map_timer:
        follow_key_map_timer(window_, delivery.map_timer);
        return;
    case DeliveryStep::follow_tab_tip_timer:
        follow_tab_tip_timer(window_, delivery.tab_tip_timer, hover_setting_);
        return;
    case DeliveryStep::announce_font_size:
        announce_settings(delivery);
        return;
    case DeliveryStep::follow_look:
        delivery = follow_look(std::move(delivery));
        return;
    case DeliveryStep::follow_split:
        follow_split(previous_layout, delivery);
        return;
    case DeliveryStep::follow_explorer:
        follow_explorer(delivery);
        return;
    case DeliveryStep::follow_minimap:
        follow_minimap(delivery);
        return;
    case DeliveryStep::follow_ime:
        follow_ime(delivery);
        return;
    }
    std::unreachable();
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

// プレビューを見せているか（側の枠か全面・正本は application の見せ方・ADR 0155 の改訂 15）。
bool EditorWindow::previewing() const
{
    return controller_.preview_placement() != application::PreviewPlacement::off;
}

bool EditorWindow::full_previewing() const
{
    return controller_.preview_placement() == application::PreviewPlacement::full;
}

void EditorWindow::stop_preview_timer()
{
    if (preview_ticking_)
    {
        KillTimer(window_, preview_timer);
        preview_ticking_ = false;
    }
}

// 枠を分けた・閉じた（鍵・一覧・Vim の `Ctrl-w`・Ex の `:q`）・境界を動かした・枠を移った（両枠の
// 大きさが違ってよい）ら、操作中の枠の行数と幅を送り直す（ADR 0148 の決定 8 と 2026-11-22 の
// 追補の決定 3）。1 枠のままの打鍵の道は比べるだけ。
void EditorWindow::follow_split(PaneArrangement previous, application::EditorDelivery &delivery)
{
    if (pane_arrangement() != previous)
    {
        delivery = controller_.apply(body_view());
    }
}

// エクスプローラーの面を出した・消した（鍵・一覧・Ex の `set explorer`）ら、本文の幅が変わるので
// 操作中の枠の行数と幅を送り直す（ADR 0162 の決定 2）。面が消えたままの打鍵の道は比べるだけ。
// 消えたら境のドラッグも終える（幅は送らない・ADR 0162 の追補 D78）。
void EditorWindow::follow_explorer(application::EditorDelivery &delivery)
{
    if (delivery.settings.explorer == shown_explorer_)
    {
        return;
    }
    shown_explorer_ = delivery.settings.explorer;
    if (!shown_explorer_)
    {
        end_hidden_explorer_drag();
    }
    delivery = controller_.apply(body_view());
}

// ミニマップを出した・消した（`set minimap`・全面プレビューの出入り）ら、同じく送り直す（ADR 0171
// の決定 2）。follow_explorer と同じ形で、出ていないままの打鍵の道は比べるだけ。
void EditorWindow::follow_minimap(application::EditorDelivery &delivery)
{
    if (controller_.minimap_shown() == shown_minimap_)
    {
        return;
    }
    shown_minimap_ = controller_.minimap_shown();
    // 押している間に面が消えた（`:set nominimap`・全面プレビュー）らドラッグは終わり（ADR 0171 の
    // 決定 5 の追補・end_hidden_explorer_drag と同じ）。
    if (!shown_minimap_)
    {
        end_minimap_drag(WM_CAPTURECHANGED);
    }
    delivery = controller_.apply(body_view());
}
} // namespace nenenib::ui::win32
