// scope `--vim-marks` の単体テスト（ADR 0042 決定 2・ADR 0152）。`%` とマークの oracle の fixture
// と、 oracle に流せない失敗（E20 E19 は -es の Vim がエラーで終わる）・CRLF・タブ・純関数の契約。
#include "Appearance.hpp"
#include "Document.hpp"
#include "DocumentState.hpp"
#include "DocumentView.hpp"
#include "Edit.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorState.hpp"
#include "FilePath.hpp"
#include "LineNumber.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "Selection.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "VimMarkEdits.hpp"
#include "VimMarkPosition.hpp"
#include "VimMarks.hpp"
#include "VimMatchingPair.hpp"
#include "VimRegisterKind.hpp"
#include "VimRepeatFailure.hpp"
#include "VimTestSupport.hpp"

#include "../vim/VimFixtures.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorState;
using nenenib::core::Appearance;
using nenenib::core::Edit;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::TextBuffer;
using nenenib::core::VimMarkPosition;
using nenenib::core::VimMarks;
using nenenib::core::VimRepeatFailure;

[[nodiscard]] TextBuffer text_of(std::string_view text)
{
    return buffer_of(text);
}

[[nodiscard]] VimMarks marked(VimMarkPosition position)
{
    VimMarks marks;
    marks.named.at(0) = position;
    return marks;
}

[[nodiscard]] std::optional<VimMarkPosition> adjusted_a(VimMarkPosition position,
                                                        std::string_view before, const Edit &edit)
{
    return nenenib::core::marks_adjusted(marked(position), text_of(std::string(before)), edit)
        .named.at(0);
}

// 編集で行がずれたマーク（決定 6）。
void verify_marks_adjusted()
{
    // dd（行の先頭から改行まで）は消えた行のマークを消し、下を上げる。
    const Edit middle{Offset{2}, "b\n", "", Offset{2}};
    expect(!adjusted_a({LineNumber{2}, 0}, "a\nb\nc", middle).has_value(),
           "dd removes the mark on the deleted line");
    expect(adjusted_a({LineNumber{3}, 0}, "a\nb\nc", middle) == VimMarkPosition{LineNumber{2}, 0},
           "dd lifts the mark below");
    expect(adjusted_a({LineNumber{1}, 0}, "a\nb\nc", middle) == VimMarkPosition{LineNumber{1}, 0},
           "dd keeps the mark above");
    // 最終行の dd は行の内容の終わりから本文の末尾まで。
    const Edit last{Offset{3}, "\nc", "", Offset{4}};
    expect(!adjusted_a({LineNumber{3}, 0}, "a\nb\nc", last).has_value() &&
               adjusted_a({LineNumber{2}, 0}, "a\nb\nc", last) == VimMarkPosition{LineNumber{2}, 0},
           "deleting the last line removes only its mark");
    // P と O は行の先頭へ改行で終わる本文を入れ、その行から下を押し下げる。i<CR> は分けた行を残す。
    const Edit above{Offset{2}, "", "x\n", Offset{2}};
    expect(adjusted_a({LineNumber{2}, 1}, "a\nbc", above) == VimMarkPosition{LineNumber{3}, 1},
           "a line put above pushes the mark down");
    const Edit split{Offset{3}, "", "\n", Offset{3}};
    expect(adjusted_a({LineNumber{2}, 1}, "a\nbc", split) == VimMarkPosition{LineNumber{2}, 1},
           "splitting a line keeps the mark on it with its column");
    // J は行を丸ごと書き直しても、改行と字下げだけの削除として自然な位置へつなぐ。
    const Edit join{Offset{0}, "ab\n  cd", "ab cd", Offset{0}};
    expect(adjusted_a({LineNumber{2}, 3}, "ab\n  cd", join) == VimMarkPosition{LineNumber{1}, 4},
           "J moves the mark of the joined line to its joined place");
    // 文字単位の行またぎの削除は Vim の op_delete と同じに桁へ残りの長さを足す（oracle の実測）。
    const Edit charwise{Offset{2}, "c\nd", "", Offset{2}};
    expect(adjusted_a({LineNumber{2}, 3}, "abc\ndefgh", charwise) ==
               VimMarkPosition{LineNumber{1}, 5},
           "a charwise delete adds the kept length to the column");
    // 同じ行の中の編集は桁を動かさない。
    const Edit within{Offset{0}, "a", "", Offset{0}};
    expect(adjusted_a({LineNumber{1}, 2}, "abc", within) == VimMarkPosition{LineNumber{1}, 2},
           "an edit inside the line keeps the column");
    // CRLF の dd も行を消す。
    const Edit crlf{Offset{3}, "b\r\n", "", Offset{3}};
    expect(!adjusted_a({LineNumber{2}, 0}, "a\r\nb\r\nc", crlf).has_value() &&
               adjusted_a({LineNumber{3}, 0}, "a\r\nb\r\nc", crlf) ==
                   VimMarkPosition{LineNumber{2}, 0},
           "dd in a CRLF text removes and lifts by lines");
}

// 最後の変更の位置と、直前の位置の確かめ（決定 5・7）。
void verify_mark_values()
{
    const auto before = text_of("a\nb\nc");
    expect(nenenib::core::vim_last_change_of(before, Edit{Offset{3}, "\nc", "", Offset{4}}) ==
               VimMarkPosition{LineNumber{3}, 0},
           "deleting the last line leaves '. below the text (E19)");
    expect(nenenib::core::vim_last_change_of(before, Edit{Offset{1}, "", "\nx", Offset{0}}) ==
               VimMarkPosition{LineNumber{2}, 0},
           "o leaves '. on the opened line");
    expect(!nenenib::core::vim_mark_offset(before, {LineNumber{4}, 0}).has_value(),
           "a mark below the text has no place");
    VimMarks jumped = nenenib::core::vim_marks_jumped(marked({LineNumber{1}, 0}),
                                                      VimMarkPosition{LineNumber{2}, 0});
    jumped = nenenib::core::vim_marks_jumped(jumped, VimMarkPosition{LineNumber{3}, 0});
    const VimMarks unmoved =
        nenenib::core::vim_marks_checked(jumped, VimMarkPosition{LineNumber{3}, 0});
    expect(unmoved.previous_context == VimMarkPosition{LineNumber{2}, 0} &&
               !unmoved.prior_context.has_value(),
           "a jump that did not move puts the previous context back");
    expect(nenenib::core::vim_mark_named(VimMarks{}, U'a').error() == VimRepeatFailure::not_found,
           "an unset mark is not found (E20)");
    expect(nenenib::core::vim_mark_set(VimMarks{}, U'.', {LineNumber{1}, 0}).error() ==
               VimRepeatFailure::refused,
           "m. is refused");
    expect(nenenib::core::vim_mark_set(VimMarks{}, U'A', {LineNumber{1}, 0}).value() == VimMarks{},
           "an uppercase mark is accepted and not kept yet");
}

// `%` は CRLF の改行を越えて探す（oracle は CRLF を流せない）。
void verify_matching_pair_crlf()
{
    const auto text = text_of("a (b\r\nc) d");
    expect(nenenib::core::vim_matching_pair(text, Offset{0}) == Offset{7},
           "% crosses a CRLF line break");
    expect(nenenib::core::vim_matching_pair(text, Offset{7}) == Offset{2},
           "% walks back across a CRLF line break");
}

// oracle に流せない失敗（-es の Vim はエラーで終わる）。:normal! と同じに後ろの鍵を流さない。
void verify_mark_failures()
{
    const auto body_after = [](std::string text, std::string_view keys)
    {
        Editing editing;
        open_vim_document(editing, std::move(text));
        vim_normal(editing.controller(), keys);
        return whole_vim_body(editing.controller());
    };
    expect(body_after("abc\ndef", "'bx") == "abc\ndef", "an unset mark fails (E20)");
    expect(body_after("a\nb\nc\nd", "jjmadd'ax") == "a\nb\nd",
           "a mark on a deleted line is gone (E20)");
    expect(body_after("a\nb\nc\nd", "Gddgg'.x") == "a\nb\nc",
           "'. after deleting the last line is below the text (E19)");
    expect(body_after("abc\ndef", "'.x") == "abc\ndef", "'. fails before any change");
    expect(body_after("abc\ndef", "m.x") == "abc\ndef", "m. is refused");
    expect(body_after("a\r\nb\r\nc\r\nd", "jjmaggdd'ax") == "b\n\nd", "a CRLF dd lifts the mark");
    expect(body_after("a (b\r\nc) d", "d%") == " d", "d% crosses a CRLF line break");
    // undo は消えたマークを戻さない（Vim は戻す・ADR 0152 の限界）。今の挙動を固定する（#790 R4）。
    expect(body_after("a\nb\nc", "jmaddu'ax") == "a\nb\nc",
           "a mark removed by dd stays removed after u (Vim restores it)");
}

// オペレータの後ろで失敗した移動は本文もレジスタも変えずにオペレータを打ち消す（#790 R3）。
void verify_operator_cancels()
{
    const auto cancelled = [](std::string text, std::string_view keys)
    {
        Editing editing;
        open_vim_document(editing, std::move(text));
        vim_replay(editing.controller(), keys);
        const bool untouched = editing.controller().vim_state().unnamed_register.value().kind ==
                               nenenib::core::VimRegisterKind::uninitialized;
        return std::pair{whole_vim_body(editing.controller()), untouched};
    };
    // 打った鍵の意味で流すので、打ち消された後の x は普通の x として効く。
    expect(cancelled("a b c", "d%x") == std::pair{std::string{" b c"}, false},
           "d% without a bracket cancels the operator");
    const auto unmatched = cancelled("a (b c", "d%");
    expect(unmatched.first == "a (b c" && unmatched.second,
           "d% without a match changes neither the text nor the register");
    const auto unset = cancelled("abc\ndef", "d'a");
    expect(unset.first == "abc\ndef" && unset.second,
           "d'a with an unset mark changes neither the text nor the register (E20)");
    expect(cancelled("abc\ndef", "d'ax").first == "bc\ndef",
           "the key after a cancelled d'a is a plain key");
}

// 編集の形から操作を当てる近似で Vim と違う例の今の値（ADR 0152 の限界・#790 R2・Q2）。直す席が
// 変えたことに気づけるように固定する。Vim の値は括弧の中。
void verify_mark_approximations()
{
    const auto body_after = [](std::string text, std::string_view keys)
    {
        Editing editing;
        open_vim_document(editing, std::move(text));
        vim_normal(editing.controller(), keys);
        return whole_vim_body(editing.controller());
    };
    expect(body_after("a\n\nc", "jmao<Esc>'aix<Esc>") == "a\n\nx\nc",
           "o on an empty line pushes its mark down (Vim keeps it: a/x//c)");
    expect(body_after("abc", "lmadd`aix<Esc>") == "x",
           "dd of the only line keeps its mark (Vim removes it: E20)");
    expect(body_after("a\nbb\ncc\nd", "jjlmagg3J`aix<Esc>") == "a xbb cc\nd",
           "3J reads as a charwise delete (Vim: a bb cxc/d)");
    expect(body_after("abc\ndef", "Glmaggv G$d`aix<Esc>").empty(),
           "a charwise delete of the whole text removes the marks (Vim keeps them: x)");
}

// 開いた時点の前の位置は 1 行目の桁 0（ADR 0152 の追補・#962）。本物の Vim 9.1（非 Ex）は開いた
// 直後の `''` を 1 行目へ跳ばす（`getpos("''")` は [0, 1, 1, 0]）。oracle の -es は最終行で開くので
// fixture にしない。新しいタブで開いた文書も同じ値で始まる。
void verify_marks_from_opening()
{
    const auto after = [](bool new_tab, std::string_view keys)
    {
        Editing editing;
        nenenib::application::EditorController &controller = editing.controller();
        open_vim_document(editing, "x\ny\nz");
        if (new_tab)
        {
            applied(controller, nenenib::application::NewTab{});
        }
        editing.files().hold_at("C:\\work\\other.txt", Bytes{"  abc\ndef\nghi"});
        applied(controller, nenenib::application::OpenDocument{
                                nenenib::core::FilePath::parse("C:\\work\\other.txt").value()});
        vim_normal(controller, keys);
        return std::pair{controller.frame(), whole_vim_body(controller)};
    };
    for (const bool new_tab : {false, true})
    {
        expect(caret_at(after(new_tab, "jj''").first, 1, 3),
               "'' with no jump goes to the first non-blank of line 1");
        expect(caret_at(after(new_tab, "jj``").first, 1, 1),
               "`` with no jump goes to line 1 column 1");
        expect(caret_at(after(new_tab, "''").first, 1, 3),
               "'' right after opening stays on line 1");
        expect(caret_at(after(new_tab, "jj''''").first, 3, 1),
               "the next '' goes back to where it was");
        expect(caret_at(after(new_tab, "''``").first, 1, 1),
               "`` after '' on line 1 goes back to column 1");
        expect(after(new_tab, "jj''x").second == "  bc\ndef\nghi",
               "the keys after '' are not dropped");
    }
}

// マークは文書の値で、タブを切り替えると文書に付いていく（決定 2）。まだ読んでいない文書と
// 新しいタブは開いた時点の値（前の位置だけ 1 行目・#962）。
void verify_marks_follow_documents()
{
    const VimMarks set = marked({LineNumber{1}, 0});
    const auto state = EditorState::create(Appearance::dark, EditMode::vim).with_vim_marks(set);
    const auto added = state.with_new_tab();
    expect(VimMarks{}.previous_context == VimMarkPosition{LineNumber{1}, 0},
           "the opening marks put the previous context on line 1 column 0");
    expect(added.vim_marks() == VimMarks{}, "a new tab starts with the opening marks");
    const auto back = added.with_switched(0);
    expect(back.vim_marks() == set, "switching back brings the document's marks");
    const auto reopened = state.with_opened(text_of("x"), state.document());
    expect(reopened.vim_marks() == VimMarks{}, "an opened document starts with the opening marks");
    const nenenib::application::Document document{std::nullopt, nenenib::core::TextEncoding::utf8,
                                                  std::size_t{0}};
    const nenenib::application::DocumentState bundle{
        text_of("y"),
        nenenib::core::collapsed_at(Offset{0}),
        nenenib::core::EditHistory::empty(),
        document,
        LineNumber{1},
        std::nullopt,
        std::nullopt,
        nenenib::application::DocumentView{
            nenenib::core::tab_title_for(std::nullopt, nenenib::core::SaveState::saved),
            std::nullopt, document.encoding, nenenib::core::SaveState::saved, std::nullopt}};
    const auto appended = state.with_appended({bundle}).with_switched(1);
    expect(appended.vim_marks() == VimMarks{} && appended.with_switched(0).vim_marks() == set,
           "a bundle read later starts with the opening marks and the first keeps its own");
}
} // namespace

void verify_vim_mark_contracts()
{
    verify_marks_adjusted();
    verify_mark_values();
    verify_matching_pair_crlf();
    verify_mark_failures();
    verify_operator_cancels();
    verify_mark_approximations();
    verify_marks_from_opening();
    verify_marks_follow_documents();
}

void verify_vim_mark_scope()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        const std::string_view name = fixture.name;
        if (name.starts_with("percent-") || name.starts_with("mark-") ||
            name.starts_with("jump-mark-") || name.starts_with("change-mark-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 112, "the scope replays the 112 oracle fixtures of % and the marks");
    verify_vim_mark_contracts();
}
} // namespace nenenib::tests
