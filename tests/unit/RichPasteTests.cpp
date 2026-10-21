// scope `--rich-paste` の契約（ADR 0157・Issue #774）。貼り方は文書の言語とクリップボードの
// HTML で決まり（paste_kind_of）、Ctrl+Shift+V は文字だけ、「HTML のまま貼り付け」は
// fragment のまま。どれも今の貼り付けの道（undo 1 単位）に流れる。OS の代わりに替え玉の
// ScriptedClipboard だけを読む（実機のクリップボードは触らない・ADR 0051 / CLAUDE.md）。
#include "ClipboardAction.hpp"
#include "ClipboardFailure.hpp"
#include "ClipboardOperation.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "Language.hpp"
#include "OpenDocument.hpp"
#include "PasteKind.hpp"
#include "Scopes.hpp"
#include "ScriptedClipboard.hpp"
#include "SelectEditMode.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::ClipboardAction;
using app::ClipboardOperation;
using app::PasteKind;
using core::Language;

constexpr std::string_view markdown_path = "C:\\work\\note.md";
constexpr std::string_view html_path = "C:\\work\\page.html";
constexpr std::string_view text_path = "C:\\work\\note.txt";
constexpr std::string_view held_text = "plain words";
constexpr std::string_view held_html = "<p>a <b>bold</b> word</p><ul><li>x</li><li>y</li></ul>";
// held_html を Markdown にした本文の行（frame の行を `|` でつないだ形）。
constexpr std::string_view converted_lines = "a **bold** word||- x|- y";
constexpr std::string_view missing_notice = "HTML が無いので文字だけを貼りました";
constexpr std::string_view unconverted_notice = "HTML を変換できないので文字だけを貼りました";

// 言語・HTML の有無 → 貼り方（決定 1）。
using KindCase = std::tuple<Language, bool, PasteKind>;

constexpr std::array<KindCase, 8> kind_cases{{
    {Language::markdown, true, PasteKind::markdown_from_html},
    {Language::markdown, false, PasteKind::text},
    {Language::html, true, PasteKind::html_as_is},
    {Language::html, false, PasteKind::text},
    {Language::plain, true, PasteKind::text},
    {Language::cpp, true, PasteKind::text},
    {Language::xml, true, PasteKind::text},
    {Language::plain, false, PasteKind::text},
}};

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

// 空の文書を開き、替え玉に文字と HTML を置く。
void open_empty(Editing &editing, std::string_view path, std::optional<std::string> html)
{
    editing.files().hold_at(std::string(path), Bytes{std::string{}});
    applied(editing.controller(), app::VisibleLines{vim_visible_lines});
    applied(editing.controller(), app::OpenDocument{path_of(path)});
    editing.clipboard().hold(std::string(held_text));
    editing.clipboard().hold_html(std::move(html));
}

[[nodiscard]] std::string pasted(Editing &editing, ClipboardOperation operation)
{
    return applied(editing.controller(), ClipboardAction{operation});
}

[[nodiscard]] std::string notice(Editing &editing)
{
    const auto &message = editing.controller().frame().command_message;
    return message.has_value() ? std::string(message.value().text()) : std::string{};
}

void verify_kind_table()
{
    for (const auto &[language, has_html, kind] : kind_cases)
    {
        expect(app::paste_kind_of(language, has_html) == kind,
               "the paste kind follows the language and the HTML on the clipboard");
    }
}

// Markdown の文書の Ctrl+V は変換した記法を貼り、undo 1 回で戻る（決定 1・8）。
void verify_markdown_paste()
{
    Editing editing;
    open_empty(editing, markdown_path, std::string(held_html));
    expect(pasted(editing, ClipboardOperation::paste) == converted_lines,
           "Ctrl+V in a Markdown document pastes the converted Markdown");
    expect(editing.clipboard().html_reads() == 1 && editing.clipboard().reads() == 0,
           "the converted paste reads the HTML once and not the text");
    expect(applied(editing.controller(), app::HistoryAction{core::HistoryDirection::undo}).empty(),
           "one undo takes the whole converted paste back");
}

// 文字の文書は HTML を読まずに文字だけ（決定 1・9）。HTML の文書は fragment のまま。
void verify_language_paste()
{
    Editing text;
    open_empty(text, text_path, std::string(held_html));
    expect(pasted(text, ClipboardOperation::paste) == held_text,
           "Ctrl+V in a text document pastes the text");
    expect(text.clipboard().html_reads() == 0, "a text document never reads the HTML");
    Editing html;
    open_empty(html, html_path, std::string("<b>x</b>"));
    expect(pasted(html, ClipboardOperation::paste) == "<b>x</b>",
           "Ctrl+V in an HTML document pastes the fragment as it is");
    Editing missing;
    open_empty(missing, markdown_path, std::nullopt);
    expect(pasted(missing, ClipboardOperation::paste) == held_text && notice(missing).empty(),
           "Ctrl+V without HTML pastes the text as before, without a notice");
    expect(missing.clipboard().html_reads() == 1 && missing.clipboard().reads() == 1,
           "without HTML the text is read after the HTML");
    Editing refused;
    open_empty(refused, markdown_path, std::nullopt);
    refused.clipboard().hold_html(std::unexpected(app::ClipboardFailure::unavailable));
    expect(pasted(refused, ClipboardOperation::paste) == held_text,
           "an unreadable HTML falls back to the text");
}

// Ctrl+Shift+V はいつも文字だけ、「HTML のまま貼り付け」はどの文書でも fragment（決定 2・3）。
void verify_plain_and_html_operations()
{
    Editing plain;
    open_empty(plain, markdown_path, std::string(held_html));
    expect(pasted(plain, ClipboardOperation::paste_plain) == held_text,
           "Ctrl+Shift+V pastes only the text even in a Markdown document");
    expect(plain.clipboard().html_reads() == 0, "Ctrl+Shift+V never reads the HTML");
    Editing as_is;
    open_empty(as_is, text_path, std::string("<i>y</i>"));
    expect(pasted(as_is, ClipboardOperation::paste_html) == "<i>y</i>",
           "paste as HTML pastes the fragment in a text document");
    expect(notice(as_is).empty(), "paste as HTML with HTML shows no notice");
    Editing missing;
    open_empty(missing, markdown_path, std::nullopt);
    expect(pasted(missing, ClipboardOperation::paste_html) == held_text,
           "paste as HTML without HTML pastes the text");
    expect(notice(missing) == missing_notice && missing.bell().rings() == 0,
           "paste as HTML without HTML says so in one line without the bell");
}

// 壊れた HTML は文字だけ貼って 1 行知らせる（決定 7・鳴らさない）。
void verify_malformed_html()
{
    Editing editing;
    open_empty(editing, markdown_path, std::string("<b"));
    expect(pasted(editing, ClipboardOperation::paste) == held_text,
           "malformed HTML pastes the text instead");
    expect(notice(editing) == unconverted_notice && editing.bell().rings() == 0,
           "malformed HTML says so in one line without the bell");
    // 変換の結果が空なら黙って文字に戻す（レビュー R3）。
    Editing empty;
    open_empty(empty, markdown_path, std::string("<script>x</script>"));
    expect(pasted(empty, ClipboardOperation::paste) == held_text && notice(empty).empty(),
           "HTML that converts to nothing pastes the text without a notice");
}

// Vim INSERT の OS の貼り付けも同じ判定。NORMAL の `"+p` は文字だけ（ADR 0051 のまま）で、
// 一覧の 2 つの操作は NORMAL では何もしない（決定 2・3）。
void verify_vim_paste()
{
    Editing insert;
    open_empty(insert, markdown_path, std::string(held_html));
    applied(insert.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(insert.controller(), "i");
    expect(pasted(insert, ClipboardOperation::paste) == converted_lines,
           "Ctrl+V in Vim INSERT converts like the ordinary mode");
    Editing normal;
    open_empty(normal, markdown_path, std::string(held_html));
    applied(normal.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(normal.controller(), "\"+p");
    expect(whole_vim_body(normal.controller()) == held_text && normal.clipboard().html_reads() == 0,
           "\"+p in Vim NORMAL pastes only the text");
    Editing listed;
    open_empty(listed, markdown_path, std::string(held_html));
    applied(listed.controller(), app::SelectEditMode{core::EditMode::vim});
    expect(pasted(listed, ClipboardOperation::paste_plain).empty() &&
               pasted(listed, ClipboardOperation::paste_html).empty(),
           "the paste operations do nothing in Vim NORMAL");
    expect(listed.clipboard().reads() == 0 && listed.clipboard().html_reads() == 0,
           "the paste operations in Vim NORMAL never read the clipboard");
    vim_replay(listed.controller(), "i");
    expect(pasted(listed, ClipboardOperation::paste_plain) == held_text,
           "Ctrl+Shift+V pastes the text in Vim INSERT");
}
} // namespace

void verify_rich_paste_contracts()
{
    verify_kind_table();
    verify_markdown_paste();
    verify_language_paste();
    verify_plain_and_html_operations();
    verify_malformed_html();
    verify_vim_paste();
}
} // namespace nenenib::tests
