// プレビューのリンクの札の application の契約（施主決定 D105・D106・#1869 の段 L2）。押しと離しが
// 同じリンクのときだけ札を開く・種類は link_destination の 1 本・Enter / Esc / Ctrl+C とボタン・
// web と mail は替え玉の LinkOpenPort へ綴りのまま・document は Nib のタブで開く・失敗と写しの 1
// 行・ ほかの使う人の操作で閉じ、合図では閉じない・表の升のリンク。実機のクリップボードとシェルは
// 触らない（ScriptedClipboard と ScriptedLinkOpen だけ）。
#include "ActOnLinkCard.hpp"
#include "ClickPreviewLink.hpp"
#include "Column.hpp"
#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "LinkCard.hpp"
#include "LinkCardAction.hpp"
#include "LinkCardTexts.hpp"
#include "LinkDestination.hpp"
#include "LinkKind.hpp"
#include "LinkOpenFailure.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewCodePiece.hpp"
#include "PreviewFrame.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewPoint.hpp"
#include "Scopes.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedLinkOpen.hpp"
#include "ScriptedMarkdown.hpp"
#include "ScrollLines.hpp"
#include "SelectEditMode.hpp"
#include "SetPreviewPlacement.hpp"
#include "TestSupport.hpp"
#include "TogglePreview.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorFrame;
using app::LinkCardAction;

[[nodiscard]] std::string message_of(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

void serve_parsed(Editing &editing)
{
    const auto &request = editing.markdown().requests().back();
    auto parsed = core::parse_markdown(*request.text, core::MarkdownStop::never());
    editing.markdown().serve(app::MarkdownOutcome{request.ticket, std::move(parsed)});
    applied(editing.controller(), app::WorkCompleted{});
}

// 保存済みのファイル（C:\work\note.txt）を開いて側のプレビューを置き、最初の券の結果を見せる。
void open_previewed(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{sample_path()});
    applied(editing.controller(), app::TogglePreview{});
    serve_parsed(editing);
}

// 見せている結果の ordinal 番目のリンク（文書の順）の始まりの点。無ければ nullopt。
[[nodiscard]] std::optional<core::PreviewPoint> link_point(const EditorFrame &frame,
                                                           std::size_t ordinal)
{
    const auto *preview =
        frame.other.has_value() ? std::get_if<app::PreviewFrame>(&frame.other.value()) : nullptr;
    if (preview == nullptr && frame.full_preview.has_value())
    {
        preview = &frame.full_preview.value();
    }
    if (preview == nullptr || preview->document == nullptr)
    {
        return std::nullopt;
    }
    std::size_t seen = 0;
    for (std::size_t block = 0; block < preview->document->blocks.size(); ++block)
    {
        const auto &links = preview->document->blocks.at(block).links;
        if (ordinal < seen + links.size())
        {
            return core::PreviewPoint{block, links.at(ordinal - seen).begin.value};
        }
        seen += links.size();
    }
    return std::nullopt;
}

// ordinal 番目のリンクの上で押して離す（同じ点）。
[[nodiscard]] EditorFrame clicked(Editing &editing, std::size_t ordinal)
{
    const auto point = link_point(editing.controller().frame(), ordinal);
    expect(point.has_value(), "the parsed preview has the link");
    return editing.controller().apply_frame(app::ClickPreviewLink{
        point.value_or(core::PreviewPoint{0, 0}), point.value_or(core::PreviewPoint{0, 0})});
}

[[nodiscard]] bool carded(const EditorFrame &frame, core::LinkKind kind, std::string_view text)
{
    return frame.link_card.has_value() && frame.link_card.value().destination.kind == kind &&
           frame.link_card.value().destination.text == text;
}

constexpr std::string_view links_text =
    "see [site](https://example.com/a) and [mail](mailto:a@example.com) or "
    "[tool](javascript:run) and [doc](other.md) and [here](#top)";

// 1. 同じリンクの押しと離しで札が開き、Enter で web は綴りのまま port へ渡って札は閉じる。失敗は
//    合図の後の 1 行。
void verify_click_opens_web()
{
    Editing editing;
    open_previewed(editing, std::string(links_text));
    const EditorFrame card = clicked(editing, 0);
    expect(carded(card, core::LinkKind::web, "https://example.com/a"),
           "clicking a web link opens the card with its spelling");
    applied(editing.controller(), app::WorkCompleted{});
    expect(editing.controller().frame().link_card.has_value(), "a signal keeps the card open");
    const EditorFrame opened =
        editing.controller().apply_frame(app::ActOnLinkCard{LinkCardAction::open});
    expect(!opened.link_card.has_value() && editing.links().uris().size() == 1 &&
               editing.links().uris().front() == "https://example.com/a",
           "Enter hands the spelling to the port and closes the card");
    editing.links().serve(std::unexpected(app::LinkOpenFailure::failed));
    const EditorFrame failed = editing.controller().apply_frame(app::WorkCompleted{});
    expect(message_of(failed) == "リンクを開けませんでした",
           "a failed hand-over says so in one line after the signal");
}

// 2. 押しと離しが違う点・違うリンク・リンクの外なら札を開かない。開いている札は閉じる。
void verify_mismatched_click_does_nothing()
{
    Editing editing;
    open_previewed(editing, std::string(links_text));
    auto &controller = editing.controller();
    const auto first = link_point(controller.frame(), 0).value_or(core::PreviewPoint{0, 0});
    const auto second = link_point(controller.frame(), 1).value_or(core::PreviewPoint{0, 0});
    expect(!controller.apply_frame(app::ClickPreviewLink{first, second}).link_card.has_value(),
           "pressing one link and releasing on another opens nothing");
    expect(!controller.apply_frame(app::ClickPreviewLink{first, core::PreviewPoint{first.block, 0}})
                .link_card.has_value(),
           "releasing outside the link opens nothing");
    static_cast<void>(clicked(editing, 0));
    expect(!controller
                .apply_frame(app::ClickPreviewLink{core::PreviewPoint{first.block, 0},
                                                   core::PreviewPoint{first.block, 0}})
                .link_card.has_value(),
           "a click off any link closes the open card");
    expect(editing.links().touches() == 0, "no click hands anything to the port");
}

// 3. Esc とボタンの左は閉じる。ほかの使う人の操作でも閉じ、窓の寸法では閉じない。
void verify_closing()
{
    Editing editing;
    open_previewed(editing, std::string(links_text));
    auto &controller = editing.controller();
    static_cast<void>(clicked(editing, 0));
    expect(!controller.apply_frame(app::ActOnLinkCard{LinkCardAction::close}).link_card.has_value(),
           "Esc closes the card");
    static_cast<void>(clicked(editing, 0));
    expect(!controller.apply_frame(app::ActOnLinkCard{LinkCardAction::secondary})
                .link_card.has_value(),
           "the left button closes the card");
    static_cast<void>(clicked(editing, 0));
    expect(controller.apply_frame(app::VisibleLines{12}).link_card.has_value(),
           "the window size keeps the card");
    const EditorFrame typed = controller.apply_frame(app::InsertText{"x"});
    expect(!typed.link_card.has_value() && editing.links().touches() == 0,
           "another key closes the card and still takes effect");
}

// 4. mail は右のボタンで書く。refused は Enter で何もせず、右のボタンは写す（閉じて 1 行）。
void verify_mail_and_refused()
{
    Editing editing;
    open_previewed(editing, std::string(links_text));
    auto &controller = editing.controller();
    expect(carded(clicked(editing, 1), core::LinkKind::mail, "mailto:a@example.com"),
           "a mail link opens the mail card");
    applied(controller, app::ActOnLinkCard{LinkCardAction::primary});
    expect(editing.links().uris().size() == 1 &&
               editing.links().uris().front() == "mailto:a@example.com",
           "the right button writes the mail through the port");
    expect(carded(clicked(editing, 2), core::LinkKind::refused, "javascript:run"),
           "another scheme opens the refused card");
    expect(controller.apply_frame(app::ActOnLinkCard{LinkCardAction::open}).link_card.has_value() &&
               editing.links().uris().size() == 1,
           "Enter on a refused card does nothing");
    const EditorFrame copied = controller.apply_frame(app::ActOnLinkCard{LinkCardAction::primary});
    const auto board = editing.clipboard().read();
    expect(!copied.link_card.has_value() && board.has_value() &&
               board.value_or(std::string{}) == "javascript:run" &&
               message_of(copied) == "宛先を写しました",
           "the right button of a refused card copies the spelling and closes");
    expect(editing.links().uris().size() == 1, "a refused destination never reaches the port");
}

// 5. Ctrl+C は写す。写せなければ ADR 0134 の 1 行で、札は開いたまま。
void verify_copy()
{
    Editing editing;
    open_previewed(editing, std::string(links_text));
    auto &controller = editing.controller();
    static_cast<void>(clicked(editing, 0));
    editing.clipboard().refuse_writes();
    const EditorFrame refused = controller.apply_frame(app::ActOnLinkCard{LinkCardAction::copy});
    expect(refused.link_card.has_value() && !message_of(refused).empty() &&
               message_of(refused) != "宛先を写しました",
           "a refused copy says so and keeps the card");
}

// 6. document は Nib のタブで開く（port は呼ばない）。見出しは札を出さない（段 L3 は 11〜14）。
void verify_document_and_heading()
{
    Editing editing;
    editing.files().hold_at("C:\\work\\other.md", Bytes{std::string("other")});
    open_previewed(editing, std::string(links_text));
    auto &controller = editing.controller();
    expect(carded(clicked(editing, 3), core::LinkKind::document, "C:\\work\\other.md"),
           "a relative path opens the file card with the resolved path");
    const EditorFrame opened = controller.apply_frame(app::ActOnLinkCard{LinkCardAction::open});
    expect(!opened.link_card.has_value() && opened.tabs.size() == 2 &&
               opened.tabs.at(opened.active_tab).title.text() == "other.md" &&
               editing.links().touches() == 0,
           "Enter opens the file in a new tab without the port");
    Editing heading;
    open_previewed(heading, std::string(links_text));
    expect(!clicked(heading, 4).link_card.has_value(), "a heading link opens no card");
}

// 7. 表の升の中のリンクも札を開く（preview_table_cell が links を写す・#1869 の L1 の申し送り）。
void verify_table_cell()
{
    Editing editing;
    open_previewed(editing, "| a | b |\n|---|---|\n| c | [d](https://d.example/) |");
    expect(carded(clicked(editing, 0), core::LinkKind::web, "https://d.example/"),
           "a link inside a table cell opens the card");
}

// 8. 本文を変えると札は閉じる（打鍵は使う人の操作・結果が変われば frame に載らない）。
void verify_edit_closes()
{
    Editing editing;
    open_previewed(editing, std::string(links_text));
    static_cast<void>(clicked(editing, 0));
    applied(editing.controller(), app::InsertText{"z"});
    applied(editing.controller(), app::WorkCompleted{});
    expect(!editing.controller().frame().link_card.has_value() &&
               !editing.controller().link_card_active(),
           "editing the document closes the card");
}

// 9. 裏の読みでアクティブなタブが替わったら札を消す（同じ形の文書が解析されても札は戻らない・
//    古い文書のフォルダで解いた宛先を開かない・#1876 のレビュー R1）。
void verify_background_tab_switch()
{
    Editing editing;
    const auto other = core::FilePath::parse("C:\\other\\note.md").value();
    editing.files().hold_at(std::string(other.text()), Bytes{std::string(links_text)});
    open_previewed(editing, std::string(links_text));
    auto &controller = editing.controller();
    editing.reads().defer_above(0);
    applied(controller, app::OpenDocument{other});
    expect(carded(clicked(editing, 3), core::LinkKind::document, "C:\\work\\other.md"),
           "the card opens while the other file is read in the background");
    editing.reads().finish(editing.reads().requests().back().ticket);
    applied(controller, app::WorkCompleted{});
    serve_parsed(editing);
    const EditorFrame switched = controller.frame();
    expect(switched.tabs.size() == 2 && !switched.link_card.has_value() &&
               !controller.link_card_active(),
           "a tab switched by the background read drops the card even for the same links");
    applied(controller, app::ActOnLinkCard{LinkCardAction::open});
    expect(controller.frame().tabs.size() == 2 && editing.links().touches() == 0,
           "Enter after the switch opens nothing");
}

// 10. 札に出す綴りは見えない字を U+FFFD に置き換え、写すのは元の綴りのまま（#1876 のレビューの軽い
// 4）。
void verify_shown_text()
{
    expect(core::link_card_shown_text("https://example.com/a") == "https://example.com/a",
           "a plain spelling is shown as it is");
    expect(core::link_card_shown_text("a\xE2\x80\xAE"
                                      "b\x01"
                                      "c") == "a\xEF\xBF\xBD"
                                              "b\xEF\xBF\xBD"
                                              "c",
           "a bidirectional control and a C0 control are shown as the replacement mark");
    expect(core::link_card_shown_text("x\xFF"
                                      "y") == "x\xEF\xBF\xBD"
                                              "y",
           "a byte of broken UTF-8 is shown as the replacement mark");
    Editing editing;
    open_previewed(editing, "[t](C:\\a\xE2\x80\xAE"
                            "exe.txt)");
    const EditorFrame card = clicked(editing, 0);
    expect(card.link_card.has_value() &&
               card.link_card.value().destination.kind == core::LinkKind::refused,
           "a path with a bidirectional control opens the refused card");
    applied(editing.controller(), app::ActOnLinkCard{LinkCardAction::copy});
    expect(editing.clipboard().read().value_or(std::string{}) == "C:\\a\xE2\x80\xAE"
                                                                 "exe.txt",
           "copying keeps the original spelling");
}
// 段 L3 の文書: 1 行目にリンク 3 つ（見出し・無い見出し・`%XX` の日本語）、7 行目と 11
// 行目が見出し。
constexpr std::string_view headings_text =
    "[go](#second-part) and [gone](#Missing) and [jp](#%E6%A6%82%E8%A6%81)\n\n# First\n\nbody\n\n"
    "## Second Part\n\nmore\n\n## \xE6\xA6\x82\xE8\xA6\x81\n";

[[nodiscard]] bool caret_at(const EditorFrame &frame, std::size_t line)
{
    return frame.caret.position.line == core::LineNumber{line} &&
           frame.caret.position.column == core::Column{1};
}

// 11. 横のプレビューの通常モード: `#見出し` は札を開かずにキャレットを見出しの行の頭へ置く。
//     `%XX` は解いて比べる。見つからなければ 1 行で、キャレットは動かない。port は呼ばない。
void verify_heading_jump_side()
{
    Editing editing;
    open_previewed(editing, std::string(headings_text));
    const EditorFrame jumped = clicked(editing, 0);
    expect(!jumped.link_card.has_value() && caret_at(jumped, 7),
           "a heading link puts the caret at the start of the heading line without a card");
    expect(caret_at(clicked(editing, 2), 11), "a percent-encoded fragment finds the heading");
    const EditorFrame missing = clicked(editing, 1);
    expect(caret_at(missing, 11) && !missing.link_card.has_value() &&
               message_of(missing) == "この文書にその見出しはありません",
           "an unknown heading says so in one line and leaves the caret");
    expect(editing.links().touches() == 0, "a heading never reaches the port");
}

// 側のプレビューの錨（見せている枠が側のプレビューでなければ nullopt）。
[[nodiscard]] std::optional<core::Offset> side_anchor(const EditorFrame &frame)
{
    const auto *preview =
        frame.other.has_value() ? std::get_if<app::PreviewFrame>(&frame.other.value()) : nullptr;
    if (preview == nullptr)
    {
        return std::nullopt;
    }
    return preview->anchor;
}

// 11b. 横のプレビューも錨を見出しに置く（レビュー R2）。見出しが本文の窓に見えていて本文が
//      送られなくても、プレビューはその見出しへ動く。
void verify_heading_jump_side_anchor()
{
    Editing editing;
    open_previewed(editing, std::string(headings_text));
    const EditorFrame jumped = clicked(editing, 0);
    const auto heading = headings_text.find("## Second Part");
    expect(jumped.first_visible == core::LineNumber{1} &&
               side_anchor(jumped) == std::optional<core::Offset>{core::Offset{heading}},
           "a visible heading moves the side preview without scrolling the body");
    expect(side_anchor(editing.controller().frame()) ==
               std::optional<core::Offset>{core::Offset{heading}},
           "the next frame keeps the side preview at the heading");
}

// 11c. 下へ遠く跳ぶと本文が送られても、プレビューの上端は見出し（本文の先頭行ではない）。本文を
//      次に送るとプレビューは追従に戻る。
void verify_heading_jump_side_far()
{
    std::string text = "[far](#far-away)\n";
    for (int line = 0; line < 40; ++line)
    {
        text += "x\n";
    }
    const auto heading = text.size();
    text += "# Far Away\n";
    for (int line = 0; line < 40; ++line)
    {
        text += "y\n";
    }
    Editing editing;
    open_previewed(editing, text);
    const EditorFrame jumped = clicked(editing, 0);
    expect(caret_at(jumped, 42) && jumped.first_visible != core::LineNumber{1} &&
               side_anchor(jumped) == std::optional<core::Offset>{core::Offset{heading}},
           "a far heading is at the top of the side preview after the body scrolls");
    const EditorFrame scrolled = editing.controller().apply_frame(app::ScrollLines{1});
    expect(side_anchor(scrolled) != std::optional<core::Offset>{core::Offset{heading}},
           "scrolling the body makes the side preview follow again");
}

// 12. Vim のモードでも同じ着地で、前の位置は `''` で戻れる（Vim の `:tag` と同じく跳ぶ命令）。
void verify_heading_jump_vim()
{
    Editing editing;
    open_previewed(editing, std::string(headings_text));
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    expect(caret_at(clicked(editing, 0), 7), "a heading link moves the Vim caret to the heading");
    applied(controller, app::VimKeyPress{core::VimKey{core::VimCharacter{U'\''}}});
    const EditorFrame back =
        controller.apply_frame(app::VimKeyPress{core::VimKey{core::VimCharacter{U'\''}}});
    expect(caret_at(back, 1), "'' goes back to the line before the jump");
}

// 12b. Vim の VISUAL と operator-pending の途中で跳ぶ（レビュー L4）。道は本文のクリックと同じ
//      PlaceCaret なので、VISUAL は本文のクリックと同じく VISUAL のまま見出しの行頭で選択を畳み、
//      待っているオペレータは捨てる（PlaceCaret だけでは残るので jump_to_heading が
//      vim_operator_cleared で捨てる・後の `k` は動くだけで消さない）。
void verify_heading_jump_vim_pending()
{
    Editing editing;
    open_previewed(editing, std::string(headings_text));
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    applied(controller, app::VimKeyPress{core::VimKey{core::VimCharacter{U'v'}}});
    const EditorFrame visual = clicked(editing, 0);
    expect(caret_at(visual, 7) && visual.vim_mode == core::VimMode::visual,
           "a heading link in VISUAL lands like a click in the body and keeps VISUAL");
    applied(controller, app::VimKeyPress{core::VimKey{core::VimCharacter{U'v'}}});
    applied(controller, app::VimKeyPress{core::VimKey{core::VimCharacter{U'd'}}});
    const EditorFrame pending = clicked(editing, 2);
    expect(caret_at(pending, 11) && pending.vim_mode == core::VimMode::normal,
           "a heading link drops a pending operator");
    const EditorFrame moved =
        controller.apply_frame(app::VimKeyPress{core::VimKey{core::VimCharacter{U'k'}}});
    expect(moved.caret.position.line == core::LineNumber{10} &&
               moved.total_lines == visual.total_lines,
           "the next motion only moves the caret");
}

// 13. 全面のプレビュー: プレビューの送りをその見出しの行へ置き、本文のキャレットも同じく動かす。
void verify_heading_jump_full()
{
    Editing editing;
    open_previewed(editing, std::string(headings_text));
    auto &controller = editing.controller();
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::full});
    const EditorFrame jumped = clicked(editing, 0);
    const auto heading = headings_text.find("## Second Part");
    expect(jumped.full_preview.has_value() &&
               jumped.full_preview.value().anchor == core::Offset{heading} && caret_at(jumped, 7) &&
               !jumped.link_card.has_value(),
           "the full preview scrolls to the heading and the caret follows");
    const EditorFrame later = controller.frame();
    expect(later.full_preview.has_value() &&
               later.full_preview.value().anchor == core::Offset{heading},
           "the next frame keeps the preview at the heading");
}

// 14. 表の升の中の `#見出し` も同じ入口で跳ぶ。
void verify_heading_jump_table_cell()
{
    Editing editing;
    open_previewed(editing, "| a |\n|---|\n| [t](#first) |\n\n# First\n");
    const EditorFrame jumped = clicked(editing, 0);
    expect(!jumped.link_card.has_value() && caret_at(jumped, 5),
           "a heading link inside a table cell jumps to the heading");
}

// 15. 札の文は種類ごと（LinkCard.hpp・#2013）。開ける札は「開く／書く・やめる・写す」、開けない札
// （heading も同じ）は「写す・閉じる」。鍵の名前は訳さない。
[[nodiscard]] bool texts_are(const core::LinkCardTexts &texts, const core::LinkCardTexts &wanted)
{
    return texts.title == wanted.title && texts.way == wanted.way && texts.hint == wanted.hint &&
           texts.secondary == wanted.secondary && texts.primary == wanted.primary;
}

void verify_card_texts()
{
    constexpr auto english = core::DisplayLanguage::english;
    expect(texts_are(core::link_card_texts(core::LinkKind::web, english),
                     {"Open this link?", "Opens in the default browser",
                      "Enter Open \xC2\xB7 Esc Cancel \xC2\xB7 Ctrl+C Copy", "Cancel", "Open"}),
           "a web card opens in the browser");
    expect(texts_are(core::link_card_texts(core::LinkKind::mail, english),
                     {"Write an email?", "Writes a new email in the default mail app",
                      "Enter Write \xC2\xB7 Esc Cancel \xC2\xB7 Ctrl+C Copy", "Cancel", "Write"}),
           "a mail card writes an email");
    expect(texts_are(core::link_card_texts(core::LinkKind::document, english),
                     {"Open this file?", "Opens in a new Nib tab",
                      "Enter Open \xC2\xB7 Esc Cancel \xC2\xB7 Ctrl+C Copy", "Cancel", "Open"}),
           "a document card opens a Nib tab");
    const core::LinkCardTexts refused{
        "This link can't be opened",
        "Programs and unknown kinds of destination aren't opened. Copy it and check it yourself",
        "Ctrl+C Copy \xC2\xB7 Esc Close", "Close", "Copy"};
    expect(texts_are(core::link_card_texts(core::LinkKind::refused, english), refused) &&
               texts_are(core::link_card_texts(core::LinkKind::heading, english), refused),
           "a refused card (and a heading, which has no card) only copies and closes");
    constexpr auto japanese = core::DisplayLanguage::japanese;
    expect(core::link_card_texts(core::LinkKind::web, japanese).title ==
                   core::ui_text(core::UiMessage::link_card_title_web, japanese) &&
               core::link_card_texts(core::LinkKind::web, japanese).primary ==
                   core::ui_text(core::UiMessage::link_card_open, japanese),
           "the card follows the display language");
}

// 16. 太字の範囲（LinkCard.hpp の link_card_emphasis）: web は `://` の後ろのホスト、mail は
// `mailto:` の後ろの宛名、document は最後の区切りの後ろ、refused は `://` があればホストで無ければ
// 最後の名前。空の範囲もある。
void verify_card_emphasis()
{
    using Range = std::pair<std::size_t, std::size_t>;
    const auto emphasis = [](core::LinkKind kind, std::string text)
    { return core::link_card_emphasis(core::LinkDestination{kind, std::move(text)}); };
    expect(emphasis(core::LinkKind::web, "https://www.vim.org/docs.php") == Range{8, 19} &&
               emphasis(core::LinkKind::web, "https://a.b?x") == Range{8, 11} &&
               emphasis(core::LinkKind::web, "https://a.b#f") == Range{8, 11} &&
               emphasis(core::LinkKind::web, "https://host") == Range{8, 12},
           "a web link shows its host up to the next / ? or #");
    expect(emphasis(core::LinkKind::mail, "mailto:me@x.org?subject=a") == Range{7, 15} &&
               emphasis(core::LinkKind::mail, "mailto:me@x.org") == Range{7, 15},
           "a mail link shows the address up to ?");
    expect(emphasis(core::LinkKind::refused, "ftp://h.example/x") == Range{6, 15} &&
               emphasis(core::LinkKind::refused, "C:\\a\\b.exe") == Range{5, 10},
           "a refused link shows its host, or its last name without ://");
    expect(emphasis(core::LinkKind::document, "C:/w/note.md") == Range{5, 12} &&
               emphasis(core::LinkKind::document, "C:/w/") == Range{5, 5},
           "a document shows its last name, which can be empty");
}

// 17. 札の幅と矩形（物理画素）。幅は上限と領域の幅 − 2 × 端の空きの小さい方で負にならない。
// 矩形はリンクの下、下にはみ出せば上、どちらにも入らなければ下端へ寄せて上端より上に出ない。
void verify_card_rect()
{
    expect(core::link_card_width(1000, 96) == 400 && core::link_card_width(300, 96) == 284 &&
               core::link_card_width(10, 96) == 0,
           "the card width is the limit or the area less its edges, never negative");
    expect(core::link_card_width(1000, 192) == 800 && core::link_card_width(500, 192) == 468,
           "the card width scales with the DPI");
    const core::LayoutRect area{0, 0, 1000, 600};
    const std::pair<std::int32_t, std::int32_t> size{400, 150};
    expect(core::link_card_rect({100, 100, 180, 120}, area, size, 96) ==
               core::LayoutRect{100, 124, 500, 274},
           "the card sits below the link after the gap");
    expect(core::link_card_rect({800, 100, 880, 120}, area, size, 96).left == 592 &&
               core::link_card_rect({2, 100, 80, 120}, area, size, 96).left == 8,
           "the card moves left before the right edge and stays inside the left edge");
    expect(core::link_card_rect({100, 500, 180, 520}, area, size, 96).top == 346,
           "a card that would leave the bottom goes above the link");
    expect(core::link_card_rect({100, 60, 180, 80}, {0, 0, 1000, 200}, size, 96).top == 50,
           "a card that fits neither below nor above is pulled up to the bottom edge");
    expect(core::link_card_rect({100, 20, 180, 40}, {0, 0, 1000, 100}, size, 96).top == 0,
           "a card taller than the area does not go above the top");
}

// 18. 位置を含むリンクの番号（[begin, end)・当たらなければ無し）。
void verify_link_at()
{
    core::PreviewBlock block{
        .kind = core::PreviewBlockKind::paragraph,
        .level = 0U,
        .quote_depth = 0U,
        .list_depth = 0U,
        .marker = {},
        .source_begin = core::Offset{0},
        .text = "0123456789",
        .spans = {},
        .task = std::nullopt,
        .table = std::nullopt,
        .alert = std::nullopt,
        .code_colors = std::nullopt,
        .links = {{core::Offset{2}, core::Offset{5}, "a"}, {core::Offset{7}, core::Offset{9}, "b"}},
        .piece = core::PreviewCodePiece::only};
    expect(core::preview_link_at(block, 2) == std::optional<std::size_t>{0} &&
               core::preview_link_at(block, 4) == std::optional<std::size_t>{0} &&
               core::preview_link_at(block, 8) == std::optional<std::size_t>{1},
           "an offset inside a link names that link");
    expect(!core::preview_link_at(block, 0).has_value() &&
               !core::preview_link_at(block, 5).has_value() &&
               !core::preview_link_at(block, 9).has_value(),
           "an offset before, between or at the end of links names none");
}
} // namespace
void verify_link_card_scope()
{
    verify_click_opens_web();
    verify_mismatched_click_does_nothing();
    verify_closing();
    verify_mail_and_refused();
    verify_copy();
    verify_document_and_heading();
    verify_table_cell();
    verify_edit_closes();
    verify_background_tab_switch();
    verify_shown_text();
    verify_heading_jump_side();
    verify_heading_jump_side_anchor();
    verify_heading_jump_side_far();
    verify_heading_jump_vim();
    verify_heading_jump_vim_pending();
    verify_heading_jump_full();
    verify_heading_jump_table_cell();
    verify_card_texts();
    verify_card_emphasis();
    verify_card_rect();
    verify_link_at();
}
} // namespace nenenib::tests
