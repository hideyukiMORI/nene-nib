// Issue #1047 / ADR 0177（N6a）。印刷の頁割りの純関数・PrintPort・意図 PrintDocument・`:hardcopy`。
// プリンタとダイアログは替え玉 ScriptedPrinter だけが受け、controller が port を呼ぶのは印刷の意図
// のときだけ。
#include "AutosaveTick.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExPrintRequest.hpp"
#include "ExResult.hpp"
#include "InsertText.hpp"
#include "Language.hpp"
#include "LineEnding.hpp"
#include "OpenDocument.hpp"
#include "PrintDocument.hpp"
#include "PrintFailure.hpp"
#include "PrintOutcome.hpp"
#include "PrintPage.hpp"
#include "PrintPages.hpp"
#include "PrintPaper.hpp"
#include "PrintRange.hpp"
#include "PrintSettings.hpp"
#include "PrintSheet.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;

constexpr std::string_view body = "int main()\r\n{\r\n}\r\n";
constexpr std::string_view too_large_notice = "印刷は 16 MiB までです";

// ui の印刷のダイアログが選んだ設定の替え玉（決定 10）。
[[nodiscard]] app::PrintDocument chosen()
{
    return app::PrintDocument{app::PrintSettings{
        "Scripted Printer", app::PrintPaper{794, 1123}, {{2, 3}}, {std::byte{0x3C}}}};
}

[[nodiscard]] std::string notice(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

// 通常モードで sample_path（note.txt）を text で開く。
void open_ordinary(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{8});
    applied(editing.controller(), app::OpenDocument{sample_path()});
}

// 決定 5: 余白 20 mm・1 ページの行数は紙の高さ − 余白 − ヘッダを行の高さで割る（整数演算）。
void verify_sheet()
{
    expect(core::print_margin(96) == 76 && core::print_margin(600) == 472 &&
               core::print_margin(0) == 0,
           "the 20 mm margin is rounded to the logical pixels of the dpi");
    // A4 の高さ 297 mm は 96 dpi で 1123 画素。(1123 − 76 × 2 − 20) / 20 = 47。
    expect(core::print_rows_per_page(core::PrintSheet{1123, 96, 20, 20}) == 47,
           "the rows per page are the height less the margins and the header over the row height");
    expect(core::print_rows_per_page(core::PrintSheet{100, 96, 20, 20}) == 1 &&
               core::print_rows_per_page(core::PrintSheet{1123, 96, 20, 2000}) == 1 &&
               core::print_rows_per_page(core::PrintSheet{1123, 96, 20, 0}) == 1,
           "a sheet that holds no whole row still carries one row per page");
}

// 決定 5: 画面行の列をページごとに割る。空の文書も 1 ページ（ヘッダの「1 / 1」を出す）。
void verify_pages()
{
    expect(core::print_pages(0, 47) == std::vector<core::PrintPage>{{0, 0}},
           "an empty document prints one page without rows");
    expect(core::print_pages(47, 47) == std::vector<core::PrintPage>{{0, 47}},
           "rows that fill one page exactly make one page");
    expect(core::print_pages(95, 47) == std::vector<core::PrintPage>{{0, 47}, {47, 47}, {94, 1}},
           "the last page carries the remaining rows");
    expect(core::print_pages(3, 0) == std::vector<core::PrintPage>{{0, 1}, {1, 1}, {2, 1}},
           "zero rows per page is treated as one");
    expect(core::print_page_label(1, 3) == "1 / 3" && core::print_page_label(12, 12) == "12 / 12",
           "the page number reads n / N");
}

// 決定 2: 意図のときだけ port を 1 回呼び、本文・改行・題名・言語・字体・行番号を写す。
void verify_job()
{
    Editing editing;
    open_ordinary(editing, std::string(body));
    applied(editing.controller(), chosen());
    const auto &jobs = editing.printer().jobs();
    expect(jobs.size() == 1, "printing asks the port once");
    const auto &settings_given = editing.printer().settings();
    expect(settings_given.size() == 1 && settings_given.front().printer == "Scripted Printer" &&
               settings_given.front().paper.width == 794 &&
               settings_given.front().ranges.size() == 1 &&
               settings_given.front().ranges.front().first == 2 &&
               settings_given.front().ticket.size() == 1,
           "the port receives the settings the dialog chose");
    const auto &job = jobs.front();
    const auto settings = core::default_editor_settings();
    expect(job.text != nullptr && *job.text == body && job.line_ending == core::LineEnding::crlf,
           "the job carries the whole body and its line ending");
    expect(job.title.text() == "note.txt" && job.language == core::Language::plain && job.gutter,
           "the job carries the file name, the language and the line numbers");
    expect(job.family.text() == settings.font_family.text() &&
               job.size.points() == settings.font_size.points(),
           "the job carries the body's font family and size");
    expect(notice(editing.controller().frame()) == "none" && editing.bell().rings() == 0,
           "a printed document says nothing and never rings");
    applied(editing.controller(), app::InsertText{"x"});
    applied(editing.controller(), chosen());
    expect(jobs.size() == 2 && jobs.back().title.text() == "note.txt" &&
               jobs.back().text->starts_with("x"),
           "a modified document prints its current body under the plain file name");
}

// 決定 2: 無題は「無題」。構文の色の言語は文書の値（`:set ft=` が変える）。
void verify_untitled_and_language()
{
    Editing editing;
    applied(editing.controller(), app::VisibleLines{8});
    applied(editing.controller(), app::InsertText{"a"});
    applied(editing.controller(), chosen());
    expect(editing.printer().jobs().size() == 1 &&
               editing.printer().jobs().front().title.text() == "無題",
           "an untitled document prints as 無題");
    open_vim_document(editing, std::string(body));
    vim_replay(editing.controller(), ":set ft=cpp<CR>");
    applied(editing.controller(), chosen());
    expect(editing.printer().jobs().size() == 2 &&
               editing.printer().jobs().back().language == core::Language::cpp,
           "the job carries the document's language");
}

// 決定 2: 取り消しは黙り、失敗は 4 種とも 1 行で知らせる（鳴らさない）。
void verify_answers()
{
    Editing editing;
    open_ordinary(editing, std::string(body));
    editing.printer().script(app::PrintOutcome::cancelled);
    applied(editing.controller(), chosen());
    expect(notice(editing.controller().frame()) == "none", "a cancelled print dialog says nothing");
    constexpr std::array<std::pair<app::PrintFailure, std::string_view>, 4> failures{{
        {app::PrintFailure::no_printer, "印刷できませんでした: プリンタがありません"},
        {app::PrintFailure::dialog_failed, "印刷できませんでした: 印刷のダイアログを開けません"},
        {app::PrintFailure::rendering_failed, "印刷できませんでした: ページを描けません"},
        {app::PrintFailure::too_large, too_large_notice},
    }};
    for (const auto &[failure, text] : failures)
    {
        editing.printer().script(std::unexpected(failure));
        applied(editing.controller(), chosen());
        expect(notice(editing.controller().frame()) == text,
               "each print failure says why in one line");
    }
    expect(editing.printer().jobs().size() == 5 && editing.bell().rings() == 0,
           "every print reaches the port once and none rings");
    // ダイアログの失敗は ui が意図に載せる。port は呼ばない（決定 10）。
    constexpr std::array<std::pair<app::PrintFailure, std::string_view>, 2> dialog_failures{{
        {app::PrintFailure::no_printer, "印刷できませんでした: プリンタがありません"},
        {app::PrintFailure::dialog_failed, "印刷できませんでした: 印刷のダイアログを開けません"},
    }};
    for (const auto &[failure, text] : dialog_failures)
    {
        applied(editing.controller(), app::PrintDocument{std::unexpected(failure)});
        expect(notice(editing.controller().frame()) == text,
               "a dialog failure says why in one line");
    }
    expect(editing.printer().jobs().size() == 5 && editing.bell().rings() == 0,
           "a dialog failure never reaches the port and never rings");
}

// 決定 7: 16 MiB を超える本文は写さず、port を呼ばずに知らせる。
void verify_too_large()
{
    Editing editing;
    constexpr std::string_view row = "0123456789abcde\n";
    std::string large;
    large.reserve(core::print_byte_limit + row.size());
    while (large.size() <= core::print_byte_limit)
    {
        large += row;
    }
    open_ordinary(editing, std::move(large));
    applied(editing.controller(), chosen());
    expect(editing.printer().jobs().empty() &&
               notice(editing.controller().frame()) == too_large_notice,
           "a body over 16 MiB is not printed and says so");
}

[[nodiscard]] std::expected<core::ExResult, core::ExEvaluationFailure> ex(std::string_view input)
{
    return core::evaluate_ex(input, core::default_editor_settings(), core::Appearance::dark);
}

// 決定 1: `:ha[rdcopy]` は同じ意図。引数・`!`・範囲は断る。
void verify_hardcopy_parse()
{
    for (const std::string_view input : {"hardcopy", "ha", "hard", "hardcopy  "})
    {
        const auto result = ex(input);
        expect(result && result.value().print == core::ExPrintRequest::active_document &&
                   !result.value().settings.has_value(),
               "hardcopy and its abbreviations ask to print without saving settings");
    }
    for (const std::string_view input : {"hardcopy!", "hardcopy > file.ps", "2hardcopy", "ha x"})
    {
        const auto result = ex(input);
        expect(!result && result.error() ==
                              core::ExEvaluationFailure{core::ExFailure::unsupported_argument},
               "hardcopy arguments, the bang and ranges are not supported yet");
    }
    const auto alone = ex("h");
    expect(!alone || !alone.value().print.has_value(), "h alone is not hardcopy");
}

// 決定 1: Vim の `:hardcopy` `:ha` と通常モードの意図は同じ道。断った `:hardcopy!` は port
// に届かない。
void verify_hardcopy()
{
    verify_hardcopy_parse();
    Editing editing;
    open_vim_document(editing, std::string(body));
    // `:hardcopy` は ui に操作「印刷」を頼むだけで、ダイアログと port は ui の道（決定 10）。
    for (const std::string_view keys : {":hardcopy<CR>", ":ha<CR>"})
    {
        vim_replay(editing.controller(), keys);
        expect(editing.controller().frame().operation_request ==
                       std::optional{core::EditorOperation::print} &&
                   editing.printer().jobs().empty(),
               "Vim's :hardcopy and :ha ask the window for the print operation");
    }
    applied(editing.controller(), chosen());
    expect(editing.printer().jobs().size() == 1 && *editing.printer().jobs().front().text == body &&
               !editing.controller().frame().operation_request.has_value(),
           "the print intent after the dialog prints the active document");
    vim_replay(editing.controller(), ":hardcopy!<CR>");
    expect(editing.printer().jobs().size() == 1 && editing.bell().rings() == 1 &&
               !editing.controller().frame().operation_request.has_value(),
           "a refused :hardcopy! never asks for the print operation");
    applied(editing.controller(), app::SelectEditMode{core::EditMode::ordinary});
    applied(editing.controller(), chosen());
    expect(editing.printer().jobs().size() == 2,
           "the ordinary mode prints through the same intent");
}

// 決定 8: 使う人の操作ではない合図・打鍵・開くは port を呼ばない。
void verify_quiet()
{
    Editing editing;
    open_ordinary(editing, std::string(body));
    applied(editing.controller(), app::InsertText{"typed"});
    applied(editing.controller(), app::AutosaveTick{});
    applied(editing.controller(), app::WorkCompleted{});
    expect(editing.printer().jobs().empty(),
           "opening, typing, the autosave tick and the work signal never print");
}
} // namespace

void verify_print_contracts()
{
    verify_sheet();
    verify_pages();
    verify_job();
    verify_untitled_and_language();
    verify_answers();
    verify_too_large();
    verify_hardcopy();
    verify_quiet();
}
} // namespace nenenib::tests
