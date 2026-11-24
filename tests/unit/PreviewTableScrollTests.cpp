// プレビューの枠より広い表の横の送り（#1883・施主決定 D107 の案 A・ADR 0151 の 2026-11-19
// の追補）。 core の寸法の純関数（端・つまみ・溝の 1 枠・ドラッグ）、parse_markdown
// の表の通し番号と数、 application
// の送りの量（表ごと・端への寄せ・幅の変化・再解析・タブの往復・つかみと載せた棒）と
// 意図の側（本文・キャレット・履歴に触れない・合図ではない・全面プレビューでも通す）。
#include "CancelSelection.hpp"
#include "ClickPreviewLink.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "HoverPreviewTable.hpp"
#include "InsertText.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "OpenSettings.hpp"
#include "PreviewFrame.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewPoint.hpp"
#include "PreviewTableBarPart.hpp"
#include "PreviewTableGesture.hpp"
#include "PreviewTableScroll.hpp"
#include "PreviewTableSpan.hpp"
#include "PreviewTick.hpp"
#include "Scopes.hpp"
#include "ScrollPreviewTable.hpp"
#include "SetPreviewPlacement.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TogglePreview.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorFrame;
using app::PreviewTableGesture;

constexpr core::PreviewTableSpan wide{100, 300};

constexpr std::string_view two_tables = "| a | b |\n|---|---|\n| 1 | 2 |\n\ntext\n\n"
                                        "| c |\n|---|\n| 3 |\n| 4 |\n";

// 1. 寸法の純関数（DIP）。
void verify_table_scroll_geometry()
{
    expect(core::preview_table_wide(wide) && !core::preview_table_wide({300, 300}) &&
               !core::preview_table_wide({0, 300}),
           "only a table wider than a non-empty frame overflows");
    expect(core::preview_table_scroll_limit(wide) == 200 &&
               core::preview_table_scroll_limit({300, 100}) == 0,
           "the limit is the table width minus the frame width");
    expect(core::preview_table_scroll_clamped(-5, wide) == 0 &&
               core::preview_table_scroll_clamped(500, wide) == 200 &&
               core::preview_table_scroll_clamped(70, wide) == 70,
           "an offset is kept between 0 and the limit");
    const auto start = core::preview_table_thumb(0, wide);
    const auto end = core::preview_table_thumb(200, wide);
    expect(start.left == 0 && start.length == 33 && end.left + end.length == 100,
           "the thumb is proportional to frame / table and reaches both ends");
    expect(core::preview_table_thumb(0, {100, 10'000}).length == core::preview_table_thumb_minimum,
           "the thumb is never shorter than the minimum");
    expect(core::preview_table_bar_part(0, 10, wide) == core::PreviewTableBarPart::thumb &&
               core::preview_table_bar_part(0, 50, wide) == core::PreviewTableBarPart::after &&
               core::preview_table_bar_part(200, 10, wide) == core::PreviewTableBarPart::before,
           "a point on the bar is before, on or after the thumb");
    expect(core::preview_table_scroll_paged(0, core::PreviewTableBarPart::after, wide) == 100 &&
               core::preview_table_scroll_paged(150, core::PreviewTableBarPart::after, wide) ==
                   200 &&
               core::preview_table_scroll_paged(50, core::PreviewTableBarPart::before, wide) == 0 &&
               core::preview_table_scroll_paged(50, core::PreviewTableBarPart::thumb, wide) == 50,
           "a press on the track moves by one frame and stops at the ends");
    expect(core::preview_table_scroll_dragged(0, 67, wide) == 200 &&
               core::preview_table_scroll_dragged(0, 1000, wide) == 200 &&
               core::preview_table_scroll_dragged(100, -1000, wide) == 0 &&
               core::preview_table_scroll_dragged(core::preview_table_scroll_dragged(40, 20, wide),
                                                  -20, wide) == 40,
           "dragging the thumb follows the pointer and returns to where it started");
}

// 2. parse_markdown は表に通し番号を振り、数を持つ。表の無い文書は 0。
void verify_table_ordinals()
{
    const auto parsed = core::parse_markdown(two_tables, core::MarkdownStop::never());
    expect(parsed.has_value() && parsed.value().tables == 2, "the document counts its tables");
    std::vector<std::uint32_t> ordinals;
    for (const auto &block : parsed.value_or(core::MarkdownDocument{}).blocks)
    {
        if (block.table.has_value())
        {
            ordinals.push_back(block.table.value().table);
        }
    }
    expect(ordinals == std::vector<std::uint32_t>{0, 0, 1, 1, 1},
           "every row carries the ordinal of its table");
    const auto plain = core::parse_markdown("just text\n", core::MarkdownStop::never());
    expect(plain.has_value() && plain.value().tables == 0, "a document without tables has none");
}

void serve_parsed(Editing &editing);

// 本文の改訂を間引きの合図 2 回で頼み直させ、結果を返す（ADR 0155 の決定 8）。
void reparsed(Editing &editing)
{
    applied(editing.controller(), app::PreviewTick{});
    applied(editing.controller(), app::PreviewTick{});
    serve_parsed(editing);
}

void serve_parsed(Editing &editing)
{
    const auto &request = editing.markdown().requests().back();
    auto parsed = core::parse_markdown(*request.text, core::MarkdownStop::never());
    editing.markdown().serve(app::MarkdownOutcome{request.ticket, std::move(parsed)});
    applied(editing.controller(), app::WorkCompleted{});
}

void open_previewed(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{sample_path()});
    applied(editing.controller(), app::TogglePreview{});
    serve_parsed(editing);
}

[[nodiscard]] const app::PreviewFrame *preview_of(const EditorFrame &frame)
{
    if (frame.full_preview.has_value())
    {
        return &frame.full_preview.value();
    }
    return frame.other.has_value() ? std::get_if<app::PreviewFrame>(&frame.other.value()) : nullptr;
}

[[nodiscard]] std::int32_t offset_of(const EditorFrame &frame, std::size_t table)
{
    const auto *preview = preview_of(frame);
    return preview != nullptr && table < preview->table_scrolls.size()
               ? preview->table_scrolls.at(table)
               : 0;
}

[[nodiscard]] std::optional<std::size_t> lit_of(const EditorFrame &frame)
{
    const auto *preview = preview_of(frame);
    return preview == nullptr ? std::nullopt : preview->table_lit;
}

[[nodiscard]] EditorFrame turned(Editing &editing, std::size_t table, std::int32_t dips,
                                 core::PreviewTableSpan span = wide)
{
    return editing.controller().apply_frame(
        app::ScrollPreviewTable{table, PreviewTableGesture::wheel, dips, span});
}

// 3. ホイールは表ごとに送り、端で止まる。収まる表と範囲の外の表は送らない。
void verify_wheel()
{
    Editing editing;
    open_previewed(editing, std::string(two_tables));
    expect(offset_of(editing.controller().frame(), 0) == 0, "a table starts unscrolled");
    expect(offset_of(turned(editing, 0, 50), 0) == 50, "the wheel scrolls the table");
    const EditorFrame far = turned(editing, 0, 1000);
    expect(offset_of(far, 0) == 200 && offset_of(far, 1) == 0,
           "the wheel stops at the end and leaves the other table");
    expect(offset_of(turned(editing, 0, -30), 0) == 170, "the wheel scrolls back");
    expect(offset_of(turned(editing, 1, 20), 1) == 20 &&
               offset_of(editing.controller().frame(), 0) == 170,
           "each table keeps its own offset");
    expect(offset_of(turned(editing, 0, 20, core::PreviewTableSpan{300, 300}), 0) == 170,
           "a table that fits is not scrolled");
    const EditorFrame outside = turned(editing, 2, 20);
    expect(preview_of(outside) != nullptr && preview_of(outside)->table_scrolls.size() <= 2,
           "a table that does not exist is not scrolled");
    // 枠が広がって端を越えた送りは端へ寄せてから動く。
    expect(offset_of(turned(editing, 0, -10, core::PreviewTableSpan{200, 300}), 0) == 90,
           "a wider frame pulls the offset back to the end before it moves");
}

// 4. 棒の押し: つまみはつかみ（明るく描く）、溝は 1 枠。ドラッグはつかんだ表だけ。離すと終わる。
void verify_bar()
{
    Editing editing;
    open_previewed(editing, std::string(two_tables));
    auto &controller = editing.controller();
    const EditorFrame paged =
        controller.apply_frame(app::ScrollPreviewTable{0, PreviewTableGesture::press, 80, wide});
    expect(offset_of(paged, 0) == 100 && !lit_of(paged).has_value(),
           "a press on the track after the thumb moves one frame without a grip");
    // 1 枠送った後のつまみは [34, 67)。
    const EditorFrame grabbed =
        controller.apply_frame(app::ScrollPreviewTable{0, PreviewTableGesture::press, 60, wide});
    expect(offset_of(grabbed, 0) == 100 && lit_of(grabbed) == std::optional<std::size_t>{0},
           "a press on the thumb grabs it and lights it");
    expect(offset_of(controller.apply_frame(
                         app::ScrollPreviewTable{1, PreviewTableGesture::drag, 90, wide}),
                     1) == 0,
           "dragging another table does nothing");
    const EditorFrame dragged =
        controller.apply_frame(app::ScrollPreviewTable{0, PreviewTableGesture::drag, 70, wide});
    expect(offset_of(dragged, 0) == 130 && lit_of(dragged) == std::optional<std::size_t>{0},
           "the thumb follows the pointer while grabbed");
    expect(offset_of(turned(editing, 0, -10), 0) == 120 && lit_of(controller.frame()).has_value(),
           "the wheel keeps the grip");
    const EditorFrame released = controller.apply_frame(
        app::ScrollPreviewTable{0, PreviewTableGesture::release, 0, core::PreviewTableSpan{0, 0}});
    expect(offset_of(released, 0) == 120 && !lit_of(released).has_value(),
           "the release ends the grip and keeps the offset");
    expect(offset_of(controller.apply_frame(
                         app::ScrollPreviewTable{0, PreviewTableGesture::drag, 0, wide}),
                     0) == 120,
           "a drag after the release does nothing");
    applied(controller, app::HoverPreviewTable{1});
    expect(lit_of(controller.frame()) == std::optional<std::size_t>{1},
           "the bar under the pointer is lit");
    expect(!controller.apply(app::HoverPreviewTable{1}).repaint &&
               lit_of(controller.frame()) == std::optional<std::size_t>{1},
           "moving over the same bar again keeps it lit without a repaint");
    applied(controller, app::HoverPreviewTable{std::nullopt});
    expect(!lit_of(controller.frame()).has_value(), "leaving the bar unlights it");
}

// 5. 再解析: 表の数が同じなら残し、変われば 0。タブを替えて戻ったら残る。
void verify_reparse_and_tabs()
{
    Editing editing;
    open_previewed(editing, std::string(two_tables));
    auto &controller = editing.controller();
    static_cast<void>(turned(editing, 0, 60));
    applied(controller, app::InsertText{"x\n\n"});
    reparsed(editing);
    expect(offset_of(controller.frame(), 0) == 60,
           "a re-parse with the same number of tables keeps the offset");
    applied(controller, app::InsertText{"| z |\n|---|\n| 9 |\n\n"});
    reparsed(editing);
    expect(offset_of(controller.frame(), 0) == 0 && offset_of(controller.frame(), 1) == 0,
           "a re-parse with another number of tables resets the offsets");
    static_cast<void>(turned(editing, 1, 40));
    applied(controller, app::NewTab{});
    expect(offset_of(controller.frame(), 1) == 0, "a new tab has no offsets");
    applied(controller, app::SwitchTab{0});
    serve_parsed(editing);
    expect(offset_of(controller.frame(), 1) == 40, "switching back keeps the offset");
}

// 6. 意図の側: 送りは本文・キャレット・履歴に触れず、リンクの札を閉じる（使う人の操作）。載せた
//    棒の知らせは札を閉じない（使う人の操作ではない）。全面プレビューでも送る。
void verify_intent_side()
{
    Editing editing;
    open_previewed(editing, "| [a](https://example.com/a) | b |\n|---|---|\n| 1 | 2 |\n");
    auto &controller = editing.controller();
    const auto revision = controller.history().revision();
    const auto text = controller.text().text();
    applied(controller, app::ClickPreviewLink{core::PreviewPoint{0, 0}, core::PreviewPoint{0, 0}});
    expect(controller.link_card_active(), "the link in the cell opens the card");
    applied(controller, app::HoverPreviewTable{0});
    expect(controller.link_card_active(), "hovering the bar keeps the card");
    static_cast<void>(turned(editing, 0, 30));
    expect(!controller.link_card_active(), "scrolling the table closes the card");
    expect(controller.history().revision() == revision && controller.text().text() == text,
           "scrolling the table touches neither the text nor the history");
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::full});
    expect(offset_of(turned(editing, 0, 20), 0) == 50, "the full preview passes the table scroll");
}
// 7. 使う人の操作の側: Ctrl+Tab の歩きを確定し（keeps_tab_walk に無い）、設定の面は閉じない
//    （only_scrolls・#1884 のレビュー L5）。
void verify_operation_side()
{
    Editing editing;
    open_previewed(editing, std::string(two_tables));
    auto &controller = editing.controller();
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    serve_parsed(editing);
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    expect(controller.tab_walking(), "Ctrl+Tab starts a walk");
    static_cast<void>(turned(editing, 0, 10));
    expect(!controller.tab_walking(), "scrolling a table settles the walk");
    applied(controller, app::SwitchTab{0});
    serve_parsed(editing);
    applied(controller, app::OpenSettings{});
    expect(controller.settings_open(), "the settings panel opens");
    const EditorFrame scrolled = turned(editing, 0, 10);
    expect(controller.settings_open() && offset_of(scrolled, 0) == 10,
           "scrolling a table keeps the settings panel open");
}
// 8. 入口で閉じた札は、送りの量が変わらない送り（端に着いた表）でも描き直して消す（#1884 の
//    再確認 M-b）。
void verify_edge_scroll_repaints()
{
    Editing editing;
    open_previewed(editing, "| [a](https://example.com/a) | b |\n|---|---|\n| 1 | 2 |\n");
    auto &controller = editing.controller();
    static_cast<void>(turned(editing, 0, 1000));
    applied(controller, app::ClickPreviewLink{core::PreviewPoint{0, 0}, core::PreviewPoint{0, 0}});
    expect(controller.link_card_active(), "the card opens over a table at its end");
    const auto delivered =
        controller.apply(app::ScrollPreviewTable{0, PreviewTableGesture::wheel, 40, wide});
    expect(delivered.repaint && !controller.link_card_active() &&
               !controller.frame().link_card.has_value() && offset_of(controller.frame(), 0) == 200,
           "a scroll that cannot move the table still repaints to take the card away");
}

// 9. つかみと載せた棒: 表の数が同じ再解析では残り、タブの往復と全面の Esc で消える（#1884 の
//    レビュー L1・L4・再確認 L-a）。
void verify_grip_lifetime()
{
    Editing editing;
    open_previewed(editing, std::string(two_tables));
    auto &controller = editing.controller();
    const auto grab = [&controller](std::int32_t at)
    {
        applied(controller, app::ScrollPreviewTable{0, PreviewTableGesture::press, at, wide});
        return lit_of(controller.frame()) == std::optional<std::size_t>{0};
    };
    expect(grab(5), "a press on the thumb grabs the table");
    applied(controller, app::InsertText{"x\n\n"});
    reparsed(editing);
    expect(lit_of(controller.frame()) == std::optional<std::size_t>{0} &&
               offset_of(controller.apply_frame(
                             app::ScrollPreviewTable{0, PreviewTableGesture::drag, 72, wide}),
                         0) == 200,
           "a re-parse with the same tables keeps the grip and the drag goes on");
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    serve_parsed(editing);
    expect(!lit_of(controller.frame()).has_value(), "switching tabs drops the grip");
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::full});
    serve_parsed(editing);
    // 送りの量は 200 のまま残り、つまみは [67, 100)。
    expect(grab(80), "the table is grabbed again in the full preview");
    applied(controller, app::CancelSelection{});
    expect(controller.frame().other.has_value() && !lit_of(controller.frame()).has_value(),
           "Esc out of the full preview drops the grip");
}
} // namespace

void verify_preview_table_scope()
{
    verify_table_scroll_geometry();
    verify_table_ordinals();
    verify_wheel();
    verify_bar();
    verify_reparse_and_tabs();
    verify_intent_side();
    verify_operation_side();
    verify_edge_scroll_repaints();
    verify_grip_lifetime();
}
} // namespace nenenib::tests
