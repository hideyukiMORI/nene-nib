// scope `--caret-motion` の単体テスト（ADR 0192・#1933）。四隅の純関数（係数の順・途中の位置・
// 時間 0 と上限・途中での行き先の変更・古い動きの進みを読まない・画面の端へ寄せる）と、時間の読みと
// 段の送り、`:set caretmotion` `caretmotiontime=`、設定の画面の行。時計・ファイルを読まない。
#include "Appearance.hpp"
#include "CaretGlide.hpp"
#include "CaretMotion.hpp"
#include "CaretMotionClock.hpp"
#include "CaretMotionTick.hpp"
#include "CaretMotionTrack.hpp"
#include "CaretQuad.hpp"
#include "Column.hpp"
#include "DisplayLanguage.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "FeatureCosts.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "MapRow.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectionAnchoring.hpp"
#include "SettingsRows.hpp"
#include "SettingsValue.hpp"
#include "SettleRecentTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimMode.hpp"
#include "VimModeSet.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using core::CaretQuad;

constexpr std::uint32_t duration = 120;

[[nodiscard]] CaretQuad cell(float left, float top)
{
    return core::caret_quad_of(left, top, left + 10.0F, top + 20.0F);
}

[[nodiscard]] core::CaretMotionTrack bounded()
{
    core::CaretMotionTrack track;
    track.bounds = core::caret_quad_of(0.0F, 0.0F, 1000.0F, 1000.0F);
    return track;
}

void verify_times()
{
    expect(core::caret_motion_time_of("120") == std::optional<std::uint16_t>{120} &&
               core::caret_motion_time_of("40") == std::optional<std::uint16_t>{40} &&
               core::caret_motion_time_of("300") == std::optional<std::uint16_t>{300},
           "times in 40-300 are read");
    expect(!core::caret_motion_time_of("39").has_value() &&
               !core::caret_motion_time_of("301").has_value() &&
               !core::caret_motion_time_of("0120").has_value() &&
               !core::caret_motion_time_of("").has_value() &&
               !core::caret_motion_time_of("12a").has_value() &&
               !core::caret_motion_time_of("-40").has_value(),
           "times outside the range or not plain decimals are refused");
    expect(core::caret_motion_time_stepped(120, true) == 140 &&
               core::caret_motion_time_stepped(120, false) == 100 &&
               core::caret_motion_time_stepped(300, true) == 300 &&
               core::caret_motion_time_stepped(40, false) == 40 &&
               core::caret_motion_time_stepped(130, true) == 140 &&
               core::caret_motion_time_stepped(130, false) == 120,
           "steps move by 20 ms, stop at the ends and snap off-step values");
    expect(core::caret_motion_time_text(120) == "120 ms", "the shown time carries the unit");
    expect(core::default_editor_settings().caret_motion_time == core::caret_motion_time_default &&
               !core::default_editor_settings().caret_motion,
           "the default is off and 120 ms");
}

void verify_shares()
{
    // 右へ進むと右の 2 隅（右上・右下）が先に着く。
    const auto right = core::caret_corner_shares(cell(0.0F, 0.0F), cell(100.0F, 0.0F));
    expect(right.at(1) < right.at(0) && right.at(2) < right.at(3) && right.at(1) < right.at(3) &&
               right.at(2) < right.at(0),
           "moving right lets the right corners arrive first");
    // 下へ進むと下の 2 隅が先。
    const auto down = core::caret_corner_shares(cell(0.0F, 0.0F), cell(0.0F, 100.0F));
    expect(down.at(2) < down.at(1) && down.at(3) < down.at(0), "moving down leads with the bottom");
    // 左上へ斜めに進むと左上の隅が最初（0.45）で、右下が最後（1.0）。
    const auto diagonal = core::caret_corner_shares(cell(100.0F, 100.0F), cell(0.0F, 0.0F));
    expect(diagonal.at(0) == 0.45F && diagonal.at(2) == 1.0F,
           "a diagonal move leads with the nearest corner and trails with the farthest");
    std::array<float, 4> sorted = right;
    float sum = 0.0F;
    for (const float share : sorted)
    {
        sum += share;
    }
    expect(sum == 0.45F + 0.6F + 0.85F + 1.0F, "each factor is used once");
}

void verify_quads()
{
    const auto from = cell(0.0F, 0.0F);
    const auto to = cell(100.0F, 0.0F);
    expect(core::caret_motion_quad(from, to, 0, duration) == from, "elapsed 0 is the start");
    expect(core::caret_motion_quad(from, to, duration, duration) == to &&
               core::caret_motion_quad(from, to, duration * 3, duration) == to,
           "the full time and beyond is the target");
    expect(core::caret_motion_quad(from, to, 10, 0) == to, "a zero time jumps");
    const auto middle = core::caret_motion_quad(from, to, duration / 2, duration);
    expect(middle.corners.at(1).x > middle.corners.at(0).x + 10.0F,
           "half way the leading corners are ahead and the quad is stretched into a trail");
    expect(middle.corners.at(3).x < to.corners.at(3).x &&
               middle.corners.at(3).x > from.corners.at(3).x,
           "half way the last corner is between its start and its end");
    // 進む隅は 0.45 × 120 = 54 ms で着く。
    const auto lead = core::caret_motion_quad(from, to, 54, duration);
    expect(lead.corners.at(1).x == to.corners.at(1).x && lead.corners.at(0).x < to.corners.at(0).x,
           "the leading corner arrives at 0.45 of the time while the tail is still moving");
    const auto grown = core::caret_quad_inflated(from, 3.0F);
    expect(grown.corners.at(0).x == -3.0F && grown.corners.at(0).y == -3.0F &&
               grown.corners.at(2).x == 13.0F && grown.corners.at(2).y == 23.0F,
           "the glow layer grows every corner outward");
}

void verify_track()
{
    const auto first = core::caret_motion_followed(bounded(), cell(0.0F, 0.0F), {}, duration);
    expect(!first.moving && first.shown == cell(0.0F, 0.0F) && first.generation == 0,
           "the first frame jumps to the caret");
    const auto still = core::caret_motion_followed(first, cell(0.0F, 0.0F), {}, duration);
    expect(!still.moving && still.shown == cell(0.0F, 0.0F), "an unmoved caret does not move");
    const auto started = core::caret_motion_followed(first, cell(100.0F, 0.0F), {}, duration);
    expect(started.moving && started.generation == 1 && started.shown == cell(0.0F, 0.0F) &&
               started.from == cell(0.0F, 0.0F),
           "a new target restarts the motion from the shown corners");
    const auto stale =
        core::caret_motion_followed(started, cell(100.0F, 0.0F), {0, duration / 2}, duration);
    expect(stale.moving && stale.shown == cell(0.0F, 0.0F),
           "the elapsed time of an older motion is read as 0");
    const auto half =
        core::caret_motion_followed(started, cell(100.0F, 0.0F), {1, duration / 2}, duration);
    expect(half.moving &&
               half.shown == core::caret_motion_quad(cell(0.0F, 0.0F), cell(100.0F, 0.0F),
                                                     duration / 2, duration),
           "the current motion advances by its elapsed time");
    // 途中で次の移動が来たら、今の四隅から次へ向かう。
    const auto redirected =
        core::caret_motion_followed(half, cell(0.0F, 200.0F), {1, 70}, duration);
    expect(redirected.moving && redirected.generation == 2 && redirected.from == half.shown,
           "a move in the middle starts from the current corners");
    const auto done =
        core::caret_motion_followed(redirected, cell(0.0F, 200.0F), {2, duration}, duration);
    expect(!done.moving && done.shown == cell(0.0F, 200.0F), "the motion ends at the target");
    const auto zero = core::caret_motion_followed(first, cell(100.0F, 0.0F), {}, 0);
    expect(!zero.moving && zero.shown == cell(100.0F, 0.0F), "a zero time jumps");
}

void verify_track_edges()
{
    auto moved = core::caret_motion_followed(bounded(), cell(0.0F, 0.0F), {}, duration);
    moved.shown = core::caret_quad_of(-50.0F, -40.0F, -40.0F, -20.0F);
    const auto clamped = core::caret_motion_followed(moved, cell(100.0F, 100.0F), {}, duration);
    expect(clamped.from.corners.at(0).x == 0.0F && clamped.from.corners.at(0).y == 0.0F,
           "a tail from outside the body starts at the edge");
    const auto cleared = core::caret_motion_settled(clamped, std::nullopt);
    expect(!cleared.target.has_value() && !cleared.moving, "a hidden caret clears the motion");
    const auto again = core::caret_motion_followed(cleared, cell(5.0F, 5.0F), {}, duration);
    expect(!again.moving && again.shown == cell(5.0F, 5.0F), "after clearing the next frame jumps");
    const auto jumped = core::caret_motion_settled(clamped, cell(7.0F, 7.0F));
    expect(!jumped.moving && jumped.shown == cell(7.0F, 7.0F) && jumped.from == cell(7.0F, 7.0F),
           "a tab switch jumps to the new caret");
    // 形の切り替え（ブロック → 縦棒）は四隅が新しい形へ向かう。
    const auto bar = core::caret_quad_of(0.0F, 2.0F, 2.0F, 18.0F);
    const auto shaped = core::caret_motion_followed(again, bar, {}, duration);
    expect(shaped.moving && shaped.from == cell(5.0F, 5.0F), "a shape change moves the corners");
}

// `:set` が置いた設定（問い・失敗は値なし）。
[[nodiscard]] std::optional<core::EditorSettings> set_by(std::string_view text,
                                                         const core::EditorSettings &settings)
{
    const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
    if (!result.has_value())
    {
        return std::nullopt;
    }
    return result.value().settings;
}

// 設定を変えない問いの答えの文（変えた・失敗は空）。
[[nodiscard]] std::string answer_of(std::string_view text, const core::EditorSettings &settings)
{
    const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
    if (!result.has_value() || result.value().settings.has_value())
    {
        return {};
    }
    return std::string(result.value().message.text());
}

void verify_settings_and_ex()
{
    const auto settings = core::default_editor_settings();
    const auto on = set_by("set caretmotion", settings);
    expect(on.has_value() && on.value_or(settings).caret_motion,
           ":set caretmotion turns the motion on");
    const auto off = set_by("set nocaretmotion", on.value_or(settings));
    expect(off.has_value() && !off.value_or(settings).caret_motion,
           ":set nocaretmotion turns it off");
    expect(answer_of("set caretmotion?", settings) == "caretmotion=off",
           ":set caretmotion? answers without changing");
    const auto timed = set_by("set caretmotiontime=200", settings);
    expect(timed.has_value() && timed.value_or(settings).caret_motion_time == 200,
           ":set caretmotiontime= sets the time");
    expect(
        !core::evaluate_ex("set caretmotiontime=20", settings, core::Appearance::dark).has_value(),
        "a time outside the range is refused");
    expect(answer_of("set caretmotiontime", settings) == "caretmotiontime=120" &&
               answer_of("set caretmotiontime?", settings) == "caretmotiontime=120",
           "the bare time name asks for the value");
    expect(core::feature_switchable(core::EditorFeature::caret_motion) &&
               !core::feature_enabled(settings, core::EditorFeature::caret_motion) &&
               core::feature_enabled(
                   core::feature_set(settings, core::EditorFeature::caret_motion, true),
                   core::EditorFeature::caret_motion),
           "the settings switch reads and writes the own key");
    const auto stepped =
        core::settings_value_stepped(settings, core::SettingsValue::caret_motion_time, true, {});
    expect(stepped.caret_motion_time == 140 &&
               core::settings_value_text(stepped, core::SettingsValue::caret_motion_time,
                                         core::DisplayLanguage::japanese) == "140 ms",
           "the settings stepper moves the time by one step");
}
// 合図は進みを frame に載せるだけで、本文・キャレット・知らせ・undo の単位を動かさない（決定 3）。
void verify_signal()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(application::InsertText{"abc"}));
    const auto before = controller.frame();
    const auto delivered = controller.apply(application::CaretMotionTick{{3, 50}});
    const auto after = controller.frame();
    expect(delivered.repaint && after.caret_motion.generation == 3 &&
               after.caret_motion.elapsed == 50,
           "the tick carries its motion number and elapsed time into the frame");
    expect(after.caret == before.caret && after.lines.size() == before.lines.size() &&
               after.document.save_state == before.document.save_state &&
               after.command_message.has_value() == before.command_message.has_value(),
           "the tick changes neither the caret, the text, the save state nor the message");
    static_cast<void>(controller.apply(application::HistoryAction{core::HistoryDirection::undo}));
    expect(controller.frame().lines.front().display.text.empty(),
           "undo after the tick still undoes the typing as one unit");
}

// 以下は合図が使う人の操作の途中の状態を動かさない契約（AutosaveTick と同じ形・#1956 の R6）。
// 合図の側に入るのは is_signal の 1 本なので、そこから外れたらここが落ちる。
constexpr application::CaretMotionTick tick{{1, 40}};

// Ctrl+Tab の歩きの途中に届いても歩きは続き、次の 1 歩は使った順の 3 つ目へ行く（ADR 0058）。
void verify_tick_keeps_tab_walk()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, application::VisibleLines{10});
    applied(controller, application::InsertText{"a"});
    for (const std::string_view body : {"b", "c", "d"})
    {
        applied(controller, application::NewTab{});
        applied(controller, application::InsertText{std::string(body)});
    }
    applied(controller, application::WalkRecentTab{core::TabStep::next});
    applied(controller, tick);
    expect(controller.tab_walking(), "the caret motion tick does not settle the walk");
    const auto third = controller.apply_frame(application::WalkRecentTab{core::TabStep::next});
    expect(!third.lines.empty() && third.lines.front().text == "b" && controller.tab_walking(),
           "the next walk after the caret motion tick reaches the third tab in the order");
    applied(controller, application::SettleRecentTab{});
}

// Vim の鍵を before と after に分けて打ち、間に合図を挟んだ結果が、挟まずに続けて打った結果と
// 同じか（本文の全体・キャレット・モード・録画）。
[[nodiscard]] bool same_with_tick(std::string_view before, std::string_view after)
{
    constexpr std::string_view text = "one two three four";
    Editing plain;
    open_vim_document(plain, std::string(text));
    vim_replay(plain.controller(), std::string(before) + std::string(after));
    Editing ticked;
    open_vim_document(ticked, std::string(text));
    vim_replay(ticked.controller(), before);
    applied(ticked.controller(), tick);
    vim_replay(ticked.controller(), after);
    const auto expected = plain.controller().frame();
    const auto actual = ticked.controller().frame();
    return whole_vim_body(plain.controller()) == whole_vim_body(ticked.controller()) &&
           expected.caret.position == actual.caret.position &&
           expected.vim_mode == actual.vim_mode && expected.recording == actual.recording;
}

// Vim の待ち（オペレータ・回数・レジスタの接頭辞）・マクロの録画・`.` の記録・undo の単位。
void verify_tick_keeps_vim_state()
{
    expect(same_with_tick("d", "w"), "an operator waiting for a motion goes on after the tick");
    expect(same_with_tick("2", "dw"), "a count goes on after the tick");
    expect(same_with_tick("\"a", "x\"ap"), "a register prefix goes on after the tick");
    expect(same_with_tick("qa", "xq@a"), "the tick never enters a macro being recorded");
    expect(same_with_tick("x", "."), "the tick never replaces the last change for dot");
    expect(same_with_tick("x", "u"), "undo after the tick undoes the same change");
    expect(same_with_tick("ia", "b<Esc>u"), "the tick never splits an insert's undo unit");
}

// `inoremap jj <Esc>` の 1 打目の待ちの途中に届いても、待ちの鍵を流さない（ADR 0176 の決定 4）。
void verify_tick_keeps_key_map_wait()
{
    auto settings = core::default_editor_settings();
    settings.maps = {
        core::MapRow{core::VimModeSet{false, true, false, false, false}, "jj", "<Esc>", false}};
    Editing editing(std::optional<core::EditorSettings>{settings});
    open_vim_document(editing, "abc\n");
    auto &controller = editing.controller();
    vim_replay(controller, "ij");
    applied(controller, tick);
    expect(controller.vim_state().mode == core::VimMode::insert &&
               vim_body(controller.frame()) == "abc\n",
           "the tick keeps the waiting j out of the body");
    vim_replay(controller, "j");
    expect(controller.vim_state().mode == core::VimMode::normal &&
               whole_vim_body(controller) == "abc\n",
           "jj after the tick still leaves INSERT without typing either j");
}

// 上下に動くときに狙う横位置を捨てない（通常モードの wanted_x と Vim の欲しい列）。
void verify_tick_keeps_wanted_column()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, application::InsertText{"abcdef\nx\nabcdef"});
    applied(controller, application::VisibleLines{10, core::TextViewGeometry{400, 20, 96}});
    applied(controller,
            application::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{5}},
                                    core::SelectionAnchoring::collapse});
    const auto down =
        application::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::collapse};
    applied(controller, down);
    applied(controller, tick);
    expect(controller.apply_frame(down).caret.position ==
               core::TextPosition{core::LineNumber{3}, core::Column{5}},
           "a caret motion tick between two downward moves keeps the wanted x");
    Editing vim;
    open_vim_document(vim, "abcdef\nx\nabcdef");
    vim_replay(vim.controller(), "4lj");
    applied(vim.controller(), tick);
    vim_replay(vim.controller(), "j");
    expect(caret_at(vim.controller().frame(), 3, 5),
           "a caret motion tick between two j keeps the Vim wanted column");
}
} // namespace

void verify_caret_motion_scope()
{
    verify_times();
    verify_shares();
    verify_quads();
    verify_track();
    verify_track_edges();
    verify_settings_and_ex();
    verify_signal();
    verify_tick_keeps_tab_walk();
    verify_tick_keeps_vim_state();
    verify_tick_keeps_key_map_wait();
    verify_tick_keeps_wanted_column();
}
} // namespace nenenib::tests
