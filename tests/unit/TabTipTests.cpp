// scope `--tab-tip` の単体テスト（#1789・ADR 0056 の #1789 の追補）。タブに載せて一定時間で出す
// 札の core（文・真ん中の切り方・置き場）と application（載せる → 合図 → 出る → 離れて消える・
// 別のタブで数え直す・無題・まだ読んでいないタブを読まない・合図では消えない・消す意図の表）。
// 時間は数えない（待ちは ui のタイマーで、ここでは合図 TabTipTick を直接送る）。
#include "AutosaveTick.hpp"
#include "BeginFind.hpp"
#include "CloseTab.hpp"
#include "Column.hpp"
#include "DevicePixels.hpp"
#include "DisplayLanguage.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorDelivery.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "PointTitleBar.hpp"
#include "RefreshAppearance.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScrollTabs.hpp"
#include "SelectEditMode.hpp"
#include "Session.hpp"
#include "SessionTab.hpp"
#include "SettleRecentTab.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TabTip.hpp"
#include "TabTipMeasure.hpp"
#include "TabTipTick.hpp"
#include "TabTipTimer.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarInput.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"
#include "TitleBarWidth.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorController;
using app::EditorFrame;
using app::PointTitleBar;
using app::TabTipTick;
using app::TabTipTimer;
using core::TitleBarHit;
using core::TitleBarTarget;

// ---------------------------------------------------------------- core

[[nodiscard]] std::optional<core::FilePath> path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

// 札の対象のタブは本体と × で同じ位置、ほかの要素は値なし。
void verify_tip_target()
{
    using core::tab_tip_tab;
    expect(tab_tip_tab(TitleBarTarget{TitleBarHit::tab, 2}) == std::optional<std::size_t>{2} &&
               tab_tip_tab(TitleBarTarget{TitleBarHit::tab_close, 2}) ==
                   std::optional<std::size_t>{2},
           "the body and the close button of a tab point at the same tab");
    expect(!tab_tip_tab(TitleBarTarget{TitleBarHit::add_tab, 0}).has_value() &&
               !tab_tip_tab(TitleBarTarget{TitleBarHit::tab_list, 0}).has_value() &&
               !tab_tip_tab(TitleBarTarget{TitleBarHit::caption, 0}).has_value() &&
               !tab_tip_tab(std::nullopt).has_value(),
           "the plus, the list, the caption and outside the band have no tab tip");
}

// 札の文はフルパスで区切りは `\`、無題は今の言語の「無題」。
void verify_tip_text()
{
    using core::DisplayLanguage;
    using core::tab_tip_text;
    expect(tab_tip_text(path_of("C:\\work\\notes\\a.txt"), DisplayLanguage::japanese) ==
               "C:\\work\\notes\\a.txt",
           "a tab with a path tips its full path");
    expect(tab_tip_text(path_of("C:/work/notes/a.txt"), DisplayLanguage::english) ==
               "C:\\work\\notes\\a.txt",
           "slashes in the path are shown as backslashes");
    expect(tab_tip_text(std::nullopt, DisplayLanguage::japanese) == "無題" &&
               tab_tip_text(std::nullopt, DisplayLanguage::english) == "Untitled",
           "an untitled tab tips the untitled text of the display language");
}

// 真ん中で切る。ドライブとファイル名が残り、全角と結合文字を割らない。
void verify_middle_clipped()
{
    using core::middle_clipped;
    using core::tab_tip_characters;
    const std::string_view path = "C:\\abcdef\\x.txt";
    expect(tab_tip_characters(path) == 15, "every ASCII byte is one character");
    expect(middle_clipped(path, 0) == path, "nothing dropped keeps the text");
    expect(middle_clipped(path, 4) == "C:\\ab…\\x.txt",
           "the middle is replaced and the tail keeps one more character");
    expect(middle_clipped(path, 15) == "…" && middle_clipped(path, 99) == "…",
           "dropping every character leaves only the mark");
    const std::string_view wide = "C:\\あいうえお\\か.txt";
    expect(tab_tip_characters(wide) == 14, "a wide character is one character");
    expect(middle_clipped(wide, 3) == "C:\\あい…\\か.txt",
           "wide characters are dropped whole and the file name stays");
    // e と結合アキュート（U+0301）は 1 文字。前後どちらに残っても離れない。
    const std::string_view combined = "ae\xCC\x81"
                                      "bce\xCC\x81z";
    expect(tab_tip_characters(combined) == 6, "a combining mark stays with its base");
    expect(middle_clipped(combined, 2) == "ae\xCC\x81…e\xCC\x81z",
           "a combining mark is never split from its base on either side");
    expect(middle_clipped(combined, 4) == "a…z", "a dropped base takes its combining mark");
    // UNC と長いパスの形（`\\server\share\…`・`\\?\C:\…`）は先頭の区切りごと残る（V-1802 の L5）。
    const auto unc = core::tab_tip_text(path_of("\\\\server\\share\\dir\\a.txt"),
                                        core::DisplayLanguage::japanese);
    expect(unc == "\\\\server\\share\\dir\\a.txt" && middle_clipped(unc, 12) == "\\\\serv…\\a.txt",
           "a UNC path keeps its leading backslashes and the file name");
    const auto extended = core::tab_tip_text(path_of("\\\\?\\C:\\deep\\folder\\b.txt"),
                                             core::DisplayLanguage::japanese);
    expect(extended == "\\\\?\\C:\\deep\\folder\\b.txt" &&
               middle_clipped(extended, 13) == "\\\\?\\C…\\b.txt",
           "an extended-length path keeps its prefix and the file name");
    // ZWJ（U+200D）は仮想桁の表で unprintable（Vim は <200d> と出す）なので、それ自身が 1 文字で
    // 絵文字に付かない（Vim の 1 文字の規則のまま）。code point は割らない。
    const std::string_view family = "a\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
                                    "\xF0\x9F\x91\xA7"
                                    "b";
    expect(tab_tip_characters(family) == 7, "a zero-width joiner is its own character");
    expect(middle_clipped(family, 5) == "a…b" && middle_clipped(family, 3) ==
                                                     "a\xF0\x9F\x91\xA8…\xF0\x9F\x91\xA7"
                                                     "b",
           "a ZWJ sequence is cut only between its emoji, never inside a code point");
}

[[nodiscard]] core::TitleBarLayout band(std::int32_t width, std::uint32_t dpi, std::size_t tabs,
                                        std::int32_t scroll)
{
    return core::title_bar_layout(core::TitleBarInput{width, dpi, tabs, 0, scroll, std::nullopt});
}

// 置き場はタブの左端・帯の下 4 DIP。右へはみ出すなら左へ寄せる。幅の上限は窓 − 16 DIP。
void verify_tip_rect()
{
    const auto layout = band(1200, 96, 3, 0);
    const auto second = core::tab_rect(layout, 1);
    const auto placed = core::tab_tip_rect(layout, 1, core::TabTipMeasure{100, 16, 96});
    expect(placed == core::LayoutRect{second.left, layout.band.bottom + 4, second.left + 120,
                                      layout.band.bottom + 4 + 28},
           "the tip is under the tab's left edge with 10 and 6 DIP of padding");
    const auto pushed = core::tab_tip_rect(layout, 2, core::TabTipMeasure{1100, 16, 96});
    expect(pushed.right == 1200 - 8 && pushed.left == 1200 - 8 - 1120,
           "a tip that would leave the window is pulled left to 8 DIP from the edge");
    expect(core::tab_tip_text_limit(1200, 96) == 1200 - 16 - 20 &&
               core::tab_tip_text_limit(1200, 144) ==
                   1200 - core::to_pixels(16, 144) - core::to_pixels(20, 144) &&
               core::tab_tip_text_limit(10, 96) == 0,
           "the text may be as wide as the window less 16 DIP and the padding");
    const auto hidpi = band(1800, 144, 3, 0);
    const auto large = core::tab_tip_rect(hidpi, 0, core::TabTipMeasure{100, 24, 144});
    expect(large.top == hidpi.band.bottom + 6 && large.right - large.left == 100 + 30 &&
               large.bottom - large.top == 24 + 18,
           "the gap and the padding scale with the DPI");
    const auto scrolled = band(400, 96, 9, 200);
    const auto clipped = core::tab_tip_rect(scrolled, 1, core::TabTipMeasure{50, 16, 96});
    expect(core::tab_rect(scrolled, 1).left < scrolled.viewport.left &&
               clipped.left == scrolled.viewport.left,
           "a tab scrolled partly out of the band tips from the band's left edge");
}

// ---------------------------------------------------------------- application

// 本文が a b c d の 4 本のタブ。アクティブは右端の d。帯の幅は 1200 DIP。
void open_four_tabs(EditorController &controller)
{
    applied(controller, app::VisibleLines{10});
    applied(controller, app::TitleBarWidth{1200});
    applied(controller, app::InsertText{"a"});
    for (const std::string_view body : {"b", "c", "d"})
    {
        applied(controller, app::NewTab{});
        applied(controller, app::InsertText{std::string(body)});
    }
}

[[nodiscard]] PointTitleBar on_tab(std::size_t tab)
{
    return PointTitleBar{TitleBarTarget{TitleBarHit::tab, tab}};
}

// 載せて待ちを始め、合図で出し、離れて消す。札の文はそのタブの経路から作る。
void verify_hover_shows_and_leaves()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    const auto pointed = controller.apply(on_tab(1));
    expect(pointed.tab_tip_timer == std::optional{TabTipTimer::start} &&
               !controller.frame().tab_tip.has_value(),
           "pointing at a tab starts the wait and shows nothing yet");
    const auto ticked = controller.apply(TabTipTick{});
    expect(ticked.repaint && !ticked.tab_tip_timer.has_value() &&
               controller.frame().tab_tip == std::optional<std::size_t>{1},
           "the wait's tick shows the tip for the pointed tab");
    expect(!controller.apply(TabTipTick{}).repaint,
           "a second tick changes nothing and asks for no repaint");
    const auto left = controller.apply(PointTitleBar{std::nullopt});
    expect(left.tab_tip_timer == std::optional{TabTipTimer::stop} &&
               !controller.frame().tab_tip.has_value(),
           "leaving the band hides the tip");
    applied(controller, on_tab(2));
    applied(controller, PointTitleBar{TitleBarTarget{TitleBarHit::add_tab, 0}});
    expect(!controller.apply(TabTipTick{}).repaint && !controller.frame().tab_tip.has_value(),
           "a tick after moving to the plus button shows nothing");
}

// 別のタブへ移ると数え直し、同じタブの本体と × の間は待ちも札も保つ。
void verify_other_tab_restarts()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, on_tab(0));
    applied(controller, TabTipTick{});
    const auto moved = controller.apply(on_tab(2));
    expect(moved.tab_tip_timer == std::optional{TabTipTimer::start} &&
               !controller.frame().tab_tip.has_value(),
           "moving to another tab hides the tip and waits again");
    applied(controller, TabTipTick{});
    expect(controller.frame().tab_tip == std::optional<std::size_t>{2},
           "the next tick shows the other tab's tip");
    const auto closer = controller.apply(PointTitleBar{TitleBarTarget{TitleBarHit::tab_close, 2}});
    expect(!closer.tab_tip_timer.has_value() &&
               controller.frame().tab_tip == std::optional<std::size_t>{2},
           "the close button of the same tab keeps the tip");
    applied(controller, on_tab(2));
    expect(controller.frame().tab_tip == std::optional<std::size_t>{2},
           "back on the same tab's body keeps the tip");
}

// 無題のタブは「無題」、経路のあるタブはフルパス（renderer が frame から作る文）。
void verify_untitled_text()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, on_tab(0));
    const EditorFrame shown = controller.apply_frame(TabTipTick{});
    expect(shown.tab_tip == std::optional<std::size_t>{0} &&
               core::tab_tip_text(shown.tabs.at(0).path, shown.language) == "無題",
           "an untitled tab's tip is the untitled text");
}

[[nodiscard]] app::SessionTab listed_tab(std::string_view name, std::size_t rank)
{
    return app::SessionTab{core::FilePath::parse("C:\\work\\" + std::string(name) + ".txt").value(),
                           core::TextPosition{core::LineNumber{1}, core::Column{1}},
                           core::LineNumber{1}, rank};
}

void hold_three(ScriptedFiles &files)
{
    files.hold_at("C:\\work\\a.txt", Bytes{std::string("a")});
    files.hold_at("C:\\work\\b.txt", Bytes{std::string("b")});
    files.hold_at("C:\\work\\c.txt", Bytes{std::string("c")});
}

// まだ読んでいないタブも、読まずにフルパスを出す（reach_tab を呼ばない）。
void verify_unloaded_tab_unread()
{
    Editing editing(SessionReading{std::optional<app::Session>{app::Session{
                        {listed_tab("a", 1), listed_tab("b", 0), listed_tab("c", 2)}, 1}}},
                    hold_three);
    EditorController &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::TitleBarWidth{1200});
    const std::size_t reads = editing.files().reads();
    applied(controller, on_tab(0));
    const EditorFrame shown = controller.apply_frame(TabTipTick{});
    expect(shown.tab_tip == std::optional<std::size_t>{0} &&
               core::tab_tip_text(shown.tabs.at(0).path, shown.language) == "C:\\work\\a.txt",
           "a tab not read yet tips its full path");
    expect(editing.files().reads() == reads && shown.active_tab == 1,
           "tipping a tab not read yet reads no file and switches nothing");
}

// 札を出したまま、合図・窓の寸法・外観・面の hover は札を消さない。
void verify_signals_keep_tip()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, on_tab(1));
    applied(controller, TabTipTick{});
    const std::vector<app::EditorIntent> kept{app::WorkCompleted{}, app::AutosaveTick{},
                                              app::VisibleLines{12}, app::TitleBarWidth{1100},
                                              app::RefreshAppearance{}};
    for (const auto &intent : kept)
    {
        const auto delivered = controller.apply(intent);
        expect(!delivered.tab_tip_timer.has_value() &&
                   controller.frame().tab_tip == std::optional<std::size_t>{1},
               "a signal or a window size keeps the tip");
    }
}

// 消す意図の表。使う人の操作は写す前に待ちと札を消し、同じタブに載ったままなら出し直さない。
void expect_dismissed(const app::EditorIntent &intent, const char *description)
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, on_tab(1));
    applied(controller, TabTipTick{});
    const auto delivered = controller.apply(intent);
    const bool hidden = delivered.tab_tip_timer == std::optional{TabTipTimer::stop} &&
                        !controller.frame().tab_tip.has_value();
    applied(controller, TabTipTick{});
    expect(hidden && !controller.frame().tab_tip.has_value(), description);
}

void verify_dismissing_intents()
{
    expect_dismissed(app::SwitchTab{1}, "pressing the tab hides the tip");
    expect_dismissed(app::InsertText{"x"}, "typing hides the tip");
    expect_dismissed(app::ScrollTabs{-1}, "the wheel on the band hides the tip");
    expect_dismissed(app::OpenCommandPalette{}, "opening the palette hides the tip");
    expect_dismissed(app::BeginFind{}, "opening the find line hides the tip");
    expect_dismissed(app::CloseTab{0}, "closing a tab hides the tip");
    expect_dismissed(app::SelectEditMode{core::EditMode::vim}, "changing the mode hides the tip");
    // × と中ボタンは押した時点で消す（離すのを待たない・D82・V-1802 の L3）。
    expect_dismissed(PointTitleBar{TitleBarTarget{TitleBarHit::tab_close, 1}, true},
                     "pressing the close button hides the tip before the release");
    expect_dismissed(PointTitleBar{TitleBarTarget{TitleBarHit::tab, 1}, true},
                     "pressing the tab with the middle button hides the tip");
    // 非アクティブの窓では合図が来ても出さない（Windows の標準のツールチップ・V-1802 の L2）。
    Editing inactive;
    open_four_tabs(inactive.controller());
    applied(inactive.controller(), on_tab(1));
    expect(!inactive.controller().apply(TabTipTick{false}).repaint &&
               !inactive.controller().frame().tab_tip.has_value() &&
               inactive.controller().frame().hovered ==
                   std::optional{TitleBarTarget{TitleBarHit::tab, 1}},
           "an inactive window shows no tip and keeps the band hover");
    applied(inactive.controller(), TabTipTick{});
    expect(!inactive.controller().frame().tab_tip.has_value(),
           "after an inactive tick the tip waits for the pointer to come back");
    // 待っている間の鍵も待ちを止める（合図が来ても出さない）。
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, on_tab(1));
    const auto typed = controller.apply(app::InsertText{"x"});
    applied(controller, TabTipTick{});
    expect(typed.tab_tip_timer == std::optional{TabTipTimer::stop} &&
               !controller.frame().tab_tip.has_value(),
           "typing while waiting stops the wait");
    applied(controller, on_tab(2));
    applied(controller, TabTipTick{});
    expect(controller.frame().tab_tip == std::optional<std::size_t>{2},
           "pointing at another tab after a dismissal waits and shows again");
}

// 面か入力行が開いている間は合図が来ても出さない。
void verify_palette_blocks_tip()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, app::OpenCommandPalette{});
    applied(controller, on_tab(1));
    expect(!controller.apply(TabTipTick{}).repaint && !controller.frame().tab_tip.has_value(),
           "the tick shows no tip while the palette is open");
}

// 合図は Ctrl+Tab の歩きを確定せず、Vim の待ち・録画・`.`・undo を動かさない（is_signal の 1 本）。
void verify_tick_is_a_signal()
{
    Editing walking;
    EditorController &controller = walking.controller();
    open_four_tabs(controller);
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    applied(controller, on_tab(0));
    applied(controller, TabTipTick{});
    expect(controller.tab_walking() && controller.frame().tab_tip == std::optional<std::size_t>{0},
           "the tick and the band hover keep the Ctrl+Tab walk");
    applied(controller, app::SettleRecentTab{});
    const auto same_with_tick = [](std::string_view before, std::string_view after)
    {
        Editing plain;
        open_vim_document(plain, "one two three");
        vim_replay(plain.controller(), std::string(before) + std::string(after));
        Editing ticked;
        open_vim_document(ticked, "one two three");
        vim_replay(ticked.controller(), before);
        applied(ticked.controller(), on_tab(0));
        applied(ticked.controller(), TabTipTick{});
        vim_replay(ticked.controller(), after);
        return whole_vim_body(plain.controller()) == whole_vim_body(ticked.controller());
    };
    expect(same_with_tick("d", "w"), "an operator waiting for a motion goes on after the tick");
    expect(same_with_tick("qa", "xq@a"), "the tick never enters a macro being recorded");
    expect(same_with_tick("x", "."), "the tick never replaces the last change for dot");
    expect(same_with_tick("ia", "b<Esc>u"), "the tick never splits an insert's undo unit");
}
} // namespace

void verify_tab_tip_contracts()
{
    verify_tip_target();
    verify_tip_text();
    verify_middle_clipped();
    verify_tip_rect();
    verify_hover_shows_and_leaves();
    verify_other_tab_restarts();
    verify_untitled_text();
    verify_unloaded_tab_unread();
    verify_signals_keep_tip();
    verify_dismissing_intents();
    verify_palette_blocks_tip();
    verify_tick_is_a_signal();
}
} // namespace nenenib::tests
