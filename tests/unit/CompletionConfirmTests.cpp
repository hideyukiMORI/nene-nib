// 契約 --completion-face の自動で開いた面の確定（Issue #1971・ADR 0190 の 2026-11-24 の追補・施主の
// 決め 2026-11-23）。triggerCharacters で開いた面で、行のクリックはその候補・Tab は選んでいればその
// 候補で選んでいなければいちばん上の候補・Enter は使う人が選んだときだけ確定する（選んでいなければ
// 改行を通して閉じる）ことを、通常モードと Vim の INSERT の両方で固定する。
// Vim の側は、確定が INSERT の undo の 1 単位の中にあること、ドットの記録とマクロの録画に打った本文
// として残り、再生で面を開かずに同じ本文になること、手で開いた面の Enter と Tab が変わらないこと、
// map より確定が先なことも縛る。替え玉 ScriptedLanguageServer だけを使い、本物のサーバーも窓も
// 使わない。
#include "CaretMotion.hpp"
#include "ChooseCompletion.hpp"
#include "CloseCompletion.hpp"
#include "CompletionCommitKey.hpp"
#include "CompletionStep.hpp"
#include "CompletionView.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "MapRow.hpp"
#include "MoveCaret.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ScrollCompletion.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "StepCompletion.hpp"
#include "TestSupport.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimModeSet.hpp"
#include "VimNamedRegisters.hpp"
#include "VimRegister.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
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

constexpr std::string_view source_path = "C:\\work\\main.cpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
constexpr std::string_view capable_answer =
    R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8","textDocumentSync":2,"completionProvider":{"triggerCharacters":[".","->"]}}}})";
constexpr std::string_view completion_method = "textDocument/completion";
// 自動の面の行は sortText の無い届いた順（empty・size・swap の順に並ぶ）。
constexpr std::string_view three_items = R"([{"label":"empty"},{"label":"size"},{"label":"swap"}])";
constexpr std::size_t shown_lines = 4;
constexpr core::VimModeSet insert_modes{false, true, false, false, false};

[[nodiscard]] SettingsReading lsp_settings(std::vector<core::MapRow> maps)
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"cpp", std::string{clangd_command}}};
    settings.maps = std::move(maps);
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

[[nodiscard]] SettingsReading lsp_settings()
{
    return lsp_settings({});
}

// main.cpp を開いて補完の能力つきで `initialize` に答え、キャレットを本文の終わりへ置く。vim なら
// Vim のモードにする（Vim の本文は 1 行・試験は `o` で 2 行目を開いて打つ）。
void capable(Editing &editing, bool vim)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string("int main() {}\n")});
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    editing.language_server().serve(app::ServerStarted{});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    editing.language_server().serve(app::ServerMessage{std::string{capable_answer}});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines}));
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::document_end, core::SelectionAnchoring::collapse});
    if (vim)
    {
        applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    }
}

[[nodiscard]] app::EditorFrame answered(Editing &editing, std::string_view result)
{
    auto &server = editing.language_server();
    server.answer(server.requests(completion_method).back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] std::size_t requests(Editing &editing)
{
    return editing.language_server().requests(completion_method).size();
}

// 窓と同じに、鍵を 1 つずつ意図 VimKeyPress で送る。最後の frame を返す。
app::EditorFrame pressed(Editing &editing, std::string_view keys)
{
    app::EditorFrame frame = editing.controller().frame();
    for (const core::VimKey &key : vim_keys_of(keys))
    {
        frame = editing.controller().apply_frame(app::VimKeyPress{key});
    }
    return frame;
}

// 選んでいる行の名前。面が無ければ "<closed>"、どの行も選んでいなければ "<none>"。
[[nodiscard]] std::string chosen(const app::EditorFrame &frame)
{
    if (!frame.completion.has_value())
    {
        return "<closed>";
    }
    const app::CompletionView &view = frame.completion.value();
    return view.selected < view.rows.size() ? view.rows.at(view.selected).label : "<none>";
}

[[nodiscard]] std::string body(Editing &editing)
{
    return vim_body(editing.controller().frame());
}

// 通常モードの本文（行は | で区切る）。
[[nodiscard]] std::string text_of(Editing &editing)
{
    return applied(editing.controller(), app::WorkCompleted{});
}

// 通常モードで 1 文字ずつ打つ（trigger は打った 1 文字で照らす）。
void typed(Editing &editing, std::string_view text)
{
    for (const char byte : text)
    {
        static_cast<void>(editing.controller().apply_frame(app::InsertText{std::string(1, byte)}));
    }
}

[[nodiscard]] bool commits(Editing &editing, app::CompletionCommitKey key)
{
    return editing.controller().completion_commits(key);
}

[[nodiscard]] std::string macro_of(Editing &editing, char32_t name)
{
    const core::VimState &vim = editing.controller().vim_state();
    return vim.registers.registers.at(core::vim_register_index(name).value_or(0)).value().text;
}

[[nodiscard]] bool inserting(Editing &editing)
{
    return editing.controller().vim_state().mode == core::VimMode::insert;
}

// 通常モードの自動の面: どの行も選ばずに出し、Enter は確定せず（窓が改行を通す）、Tab は先頭の
// 候補で確定する。↓ は先頭・↑ は最後の行を選び、選んだ後の Enter は確定する。ホイールは選ばない。
void verify_ordinary()
{
    Editing editing(lsp_settings());
    capable(editing, false);
    typed(editing, "x.");
    const auto shown = answered(editing, three_items);
    expect(chosen(shown) == "<none>" && !commits(editing, app::CompletionCommitKey::enter) &&
               commits(editing, app::CompletionCommitKey::tab),
           "an automatic face shows no chosen row, Enter does not commit and Tab does");
    const auto scrolled = editing.controller().apply_frame(app::ScrollCompletion{1});
    expect(chosen(scrolled) == "<none>", "the wheel does not choose a row");
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{}));
    expect(text_of(editing) == "int main() {}|x.empty" && !editing.controller().completion_open(),
           "Tab with no chosen row commits the top candidate");
    applied(editing.controller(), app::HistoryAction{core::HistoryDirection::undo});
    expect(text_of(editing) == "int main() {}|x.", "the commit is one undo unit");

    typed(editing, " y.");
    static_cast<void>(answered(editing, three_items));
    const auto down =
        editing.controller().apply_frame(app::StepCompletion{app::CompletionStep::next});
    expect(chosen(down) == "empty" && commits(editing, app::CompletionCommitKey::enter),
           "Down from no choice chooses the top row and Enter then commits");
    static_cast<void>(editing.controller().apply_frame(app::CloseCompletion{}));
    typed(editing, " z.");
    static_cast<void>(answered(editing, three_items));
    const auto up =
        editing.controller().apply_frame(app::StepCompletion{app::CompletionStep::previous});
    expect(chosen(up) == "swap", "Up from no choice chooses the last row");
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{}));
    typed(editing, " w.");
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{std::size_t{1}}));
    expect(text_of(editing) == "int main() {}|x. y. z.swap w.size",
           "a click on a row of an automatic face commits that row");
}

// Vim の Tab: 何も選んでいなければ先頭の候補、Ctrl-n で選んでいればその候補で確定し、Tab の文字は
// 入れない。INSERT のまま。`.` の記録は打った文字 ＋ 確定した語で、undo は INSERT の 1 単位。
void verify_vim_tab()
{
    Editing editing(lsp_settings());
    capable(editing, true);
    static_cast<void>(pressed(editing, "ox."));
    static_cast<void>(answered(editing, three_items));
    const auto top = pressed(editing, "<Tab>");
    expect(!top.completion.has_value() && !editing.controller().completion_open() &&
               inserting(editing) && body(editing) == "int main() {}\nx.empty\n",
           "Tab with no chosen row puts the top candidate in and stays in INSERT");
    static_cast<void>(pressed(editing, "<Esc>"));
    expect(dot_record_is(editing.controller().vim_state(), std::nullopt, "ox.empty<Esc>"),
           "the dot record holds the typed characters and the committed word");
    const std::size_t asked = requests(editing);
    static_cast<void>(pressed(editing, "."));
    expect(body(editing) == "int main() {}\nx.empty\nx.empty\n" && requests(editing) == asked &&
               !editing.controller().completion_open(),
           "the dot command puts the same text in without asking or opening a face");
    static_cast<void>(pressed(editing, "uu"));
    expect(body(editing) == "int main() {}\n", "the insert with its commit is one undo unit");

    static_cast<void>(pressed(editing, "oy."));
    static_cast<void>(answered(editing, three_items));
    const auto stepped = pressed(editing, "<C-n><C-n>");
    expect(chosen(stepped) == "size", "Ctrl-n twice chooses the second row");
    const auto committed = pressed(editing, "<Tab>");
    expect(!committed.completion.has_value() && inserting(editing) &&
               body(editing) == "int main() {}\ny.size\n",
           "Tab after Ctrl-n commits the chosen candidate without a tab character");
    static_cast<void>(pressed(editing, "<Esc>oz."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n><C-n><C-n><C-n>"));
    const auto original = pressed(editing, "<Tab>");
    expect(!original.completion.has_value() && body(editing) == "int main() {}\ny.size\nz.empty\n",
           "Tab after Ctrl-n walked back to the prefix commits the top candidate");
}

// Vim の Enter: Ctrl-n で選んだときだけ確定し（改行を入れない）、選んでいなければ（開いた直後・輪
// の端の元の接頭辞）Vim のまま改行を入れて閉じる。
void verify_vim_enter()
{
    Editing editing(lsp_settings());
    capable(editing, true);
    static_cast<void>(pressed(editing, "ox."));
    static_cast<void>(answered(editing, three_items));
    const auto committed = pressed(editing, "<C-n><CR>");
    expect(!committed.completion.has_value() && inserting(editing) &&
               body(editing) == "int main() {}\nx.empty\n",
           "Enter after Ctrl-n commits the chosen candidate without breaking the line");
    static_cast<void>(pressed(editing, " y."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n><C-n><C-n><C-n>"));
    const auto broken = pressed(editing, "<CR>");
    expect(!broken.completion.has_value() && !editing.controller().completion_open() &&
               body(editing) == "int main() {}\nx.empty y.\n\n",
           "Enter with the prefix chosen again breaks the line as in Vim");
    static_cast<void>(pressed(editing, "<Esc>"));
    expect(dot_record_is(editing.controller().vim_state(), std::nullopt, "ox.empty y.<CR><Esc>"),
           "the dot record holds the committed word and the line break");
}

// Vim の行のクリック: 何も選んでいない面の行はその候補で確定する（INSERT のまま・`.` の記録・undo
// の 1 単位）。engine の補完へ移った面の行も同じ 1 歩。
void verify_vim_click()
{
    Editing editing(lsp_settings());
    capable(editing, true);
    static_cast<void>(pressed(editing, "ox."));
    static_cast<void>(answered(editing, three_items));
    const auto clicked = editing.controller().apply_frame(app::ChooseCompletion{std::size_t{1}});
    expect(!clicked.completion.has_value() && !editing.controller().completion_open() &&
               inserting(editing) && body(editing) == "int main() {}\nx.size\n",
           "a click on a row of a face with no chosen row puts that candidate in");
    static_cast<void>(pressed(editing, "<Esc>"));
    expect(dot_record_is(editing.controller().vim_state(), std::nullopt, "ox.size<Esc>"),
           "the clicked candidate is in the dot record as typed text");
    static_cast<void>(pressed(editing, "u"));
    expect(body(editing) == "int main() {}\n", "the click is part of the one undo unit");
    static_cast<void>(pressed(editing, "oy."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n>"));
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{std::size_t{2}}));
    expect(body(editing) == "int main() {}\ny.swap\n" && inserting(editing),
           "a click after Ctrl-n puts the clicked candidate in");
}

// 録画: 確定した語は打った本文として積み、補完の鍵（Ctrl-n・Ctrl-p・Ctrl-e・Ctrl-y）と確定の
// Tab と Enter は積まない。再生は面を開かず要求も出さずに、打ったときと同じ本文を入れる。
void verify_vim_recording()
{
    Editing editing(lsp_settings());
    capable(editing, true);
    static_cast<void>(pressed(editing, "qaox."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<Tab> y."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n><C-n><C-p><C-n><CR> z."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{std::size_t{2}}));
    static_cast<void>(pressed(editing, " w."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n><C-y> v."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n>( u."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n><C-e><Esc>q"));
    const std::string typed = "x.empty y.size z.swap w.empty v.empty( u.";
    expect(macro_of(editing, U'a') == "o" + typed + "\x1b" &&
               body(editing) == "int main() {}\n" + typed + "\n",
           "the macro holds the committed words as typed text and none of the completion keys");
    const std::size_t asked = requests(editing);
    static_cast<void>(pressed(editing, "@a"));
    expect(body(editing) == "int main() {}\n" + typed + "\n" + typed + "\n" &&
               requests(editing) == asked && !editing.controller().completion_open(),
           "replaying the macro puts the same text in without asking or opening a face");
}

// 手で開いた面（Vim の Ctrl-n）の Enter と Tab は変わらない（Enter は語を残して改行・Tab は文字）。
// `inoremap <Tab>` があっても、自動の面が出ていれば Tab は確定で、面が無ければ map のまま。
void verify_vim_unchanged_and_map()
{
    Editing editing(lsp_settings());
    capable(editing, true);
    static_cast<void>(pressed(editing, "omap ma<C-n>"));
    static_cast<void>(answered(editing, "[]"));
    static_cast<void>(pressed(editing, "<CR>"));
    expect(body(editing) == "int main() {}\nmap main\n\n",
           "Enter on a face opened by hand keeps the word and breaks the line");
    static_cast<void>(pressed(editing, "ma<C-n>"));
    static_cast<void>(answered(editing, "[]"));
    static_cast<void>(pressed(editing, "<Tab>"));
    expect(body(editing) == "int main() {}\nmap main\nmain\t\n",
           "Tab on a face opened by hand keeps the word and puts a tab in");

    Editing mapped(lsp_settings({core::MapRow{insert_modes, "<Tab>", "Q", false}}));
    capable(mapped, true);
    static_cast<void>(pressed(mapped, "ox."));
    static_cast<void>(answered(mapped, three_items));
    static_cast<void>(pressed(mapped, "<Tab>"));
    expect(body(mapped) == "int main() {}\nx.empty\n",
           "with a face shown Tab commits before an insert map of Tab");
    static_cast<void>(pressed(mapped, "<Tab>"));
    expect(body(mapped) == "int main() {}\nx.emptyQ\n", "with no face the map of Tab still works");
}

// 12 件の候補（wa〜wl・届いた順）。
[[nodiscard]] std::string twelve_items()
{
    std::string items = "[";
    for (char letter = 'a'; letter <= 'l'; ++letter)
    {
        items += std::string(letter == 'a' ? "" : ",") + R"({"label":"w)" + std::string(1, letter) +
                 R"("})";
    }
    return items + "]";
}

// 通常モードの選んでいない面の PageDown は面の行数ぶん、PageUp は先頭を選ぶ。先頭の候補が打った語と
// 同じ綴りなら、Tab は確定せず（窓が Tab の文字を通す・#1844 の L1）。
void verify_ordinary_pages()
{
    Editing editing(lsp_settings());
    capable(editing, false);
    typed(editing, "x.");
    static_cast<void>(answered(editing, twelve_items()));
    const auto paged =
        editing.controller().apply_frame(app::StepCompletion{app::CompletionStep::page_next});
    expect(chosen(paged) == "wh", "PageDown from no choice chooses the row a face height down");
    static_cast<void>(editing.controller().apply_frame(app::CloseCompletion{}));
    typed(editing, " y.");
    static_cast<void>(answered(editing, twelve_items()));
    const auto top =
        editing.controller().apply_frame(app::StepCompletion{app::CompletionStep::page_previous});
    expect(chosen(top) == "wa", "PageUp from no choice chooses the top row");
    static_cast<void>(editing.controller().apply_frame(app::CloseCompletion{}));
    typed(editing, " z.");
    static_cast<void>(answered(editing, three_items));
    typed(editing, "size");
    expect(!commits(editing, app::CompletionCommitKey::tab),
           "Tab does not commit a top candidate spelled as the typed word");
}

// engine の語に直せない候補（textEdit
// の範囲が語の頭より前から始まり、本文と合わない）の行のクリックは 面を閉じるだけで、Tab
// は直せる候補のいちばん上で確定する。
void verify_vim_unconvertible()
{
    constexpr std::string_view items =
        R"([{"label":"bad","textEdit":{"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":2}},"newText":"zz"}},{"label":"good"}])";
    Editing editing(lsp_settings());
    capable(editing, true);
    static_cast<void>(pressed(editing, "ox."));
    static_cast<void>(answered(editing, items));
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{std::size_t{0}}));
    expect(
        !editing.controller().completion_open() && inserting(editing) &&
            body(editing) == "int main() {}\nx.\n",
        "a click on a candidate that cannot be written from the word start only closes the face");
    static_cast<void>(pressed(editing, "<BS>."));
    static_cast<void>(answered(editing, items));
    static_cast<void>(pressed(editing, "<Tab>"));
    expect(body(editing) == "int main() {}\nx.good\n",
           "Tab commits the top candidate that can be written");
}

// 左辺の待ちが積まれている間の Tab は確定せず表を引く。`inoremap <CR>` は、選んでいない面では map
// の まま、選んだ面では確定が先。
void verify_vim_pending_and_enter_map()
{
    Editing editing(lsp_settings({core::MapRow{insert_modes, "jk", "<Esc>", false},
                                  core::MapRow{insert_modes, "<CR>", "Q", false}}));
    capable(editing, true);
    static_cast<void>(pressed(editing, "ox."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "j<Tab>"));
    expect(body(editing) == "int main() {}\nx.j\t\n" && !editing.controller().completion_open(),
           "Tab after a waiting key of a map does not commit and goes through the map table");
    static_cast<void>(pressed(editing, "<Esc>oy."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<CR>"));
    expect(body(editing) == "int main() {}\nx.j\t\ny.Q\n",
           "Enter on a face with no chosen row keeps an insert map of Enter");
    static_cast<void>(pressed(editing, "<Esc>oz."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n><CR>"));
    expect(body(editing) == "int main() {}\nx.j\t\ny.Q\nz.empty\n",
           "Enter after Ctrl-n commits before an insert map of Enter");
}

// engine の外からの閉じ（窓の非アクティブなど）と、Ctrl-n の後の Esc の録画も、確定した語を打った
// 本文として持つ。選んだ候補が元の接頭辞と同じ綴りなら、Enter は確定せずに改行を入れる。
// Shift+Tab は窓が先に面を閉じるので、Tab の文字を入れる。
void verify_vim_outside_close_and_same_word()
{
    Editing editing(lsp_settings());
    capable(editing, true);
    static_cast<void>(pressed(editing, "qaox."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n>"));
    static_cast<void>(editing.controller().apply_frame(app::CloseCompletion{}));
    static_cast<void>(pressed(editing, " y."));
    static_cast<void>(answered(editing, three_items));
    static_cast<void>(pressed(editing, "<C-n><C-n><Esc>q"));
    expect(macro_of(editing, U'a') == "ox.empty y.size\x1b",
           "an outside close and Esc after Ctrl-n record the words as typed text");
    static_cast<void>(pressed(editing, "@a"));
    expect(body(editing) == "int main() {}\nx.empty y.size\nx.empty y.size\n",
           "the replay puts the same text in");

    Editing same(lsp_settings());
    capable(same, true);
    static_cast<void>(pressed(same, "ox."));
    static_cast<void>(answered(same, three_items));
    static_cast<void>(pressed(same, "size<C-n><CR>"));
    expect(body(same) == "int main() {}\nx.size\n\n",
           "Enter on a chosen candidate spelled as the prefix breaks the line");
    static_cast<void>(pressed(same, "y."));
    static_cast<void>(answered(same, three_items));
    static_cast<void>(same.controller().apply_frame(app::CloseCompletion{}));
    static_cast<void>(pressed(same, "<Tab>"));
    expect(body(same) == "int main() {}\nx.size\ny.\t\n",
           "Shift+Tab (the window closes the face first) puts a tab character in");
}
} // namespace

void verify_completion_confirm()
{
    verify_ordinary();
    verify_vim_tab();
    verify_vim_enter();
    verify_vim_click();
    verify_vim_recording();
    verify_vim_unchanged_and_map();
    verify_ordinary_pages();
    verify_vim_unconvertible();
    verify_vim_pending_and_enter_map();
    verify_vim_outside_close_and_same_word();
}
} // namespace nenenib::tests
