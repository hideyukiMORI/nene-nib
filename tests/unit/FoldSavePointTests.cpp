// scope `--fold-save-point` の単体テスト（#1672・ADR 0046 の追補）。再生（`@a`・`[count]@a`）は
// 終わりに積んだ編集を 1 つの undo の単位に畳み（merge_replayed_edits）、矩形の INSERT の Esc は
// 写した行を 1 行目の INSERT の単位に畳む（fold_block_into_insert）。畳む前の本文の位置で覚えた
// 保存時点を写さないと、ファイルに無い本文を保存済みと判定した。替え玉の
// ファイルの port に書かれたバイト列と、本文・未保存の印・閉じるときに数える未保存のタブ
// （exit_plan に渡る数）・自動退避の合図が書くかを比べる。守る性質: ファイルと本文が違えば未保存。
#include "AutosaveTick.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "FileFailure.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedAutosave.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "StoreVimRegister.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "UnsavedTab.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

constexpr std::string_view original = "abc\n";

// 今の文書の経路のファイルの中身。書かれていなければ開いたときの中身。
[[nodiscard]] std::string file_of(Editing &editing, std::string_view initial)
{
    const auto &files = editing.files();
    const auto path = editing.controller().frame().document.path;
    if (path.has_value() && files.written_path() == path.value().text())
    {
        return files.written();
    }
    return std::string(initial);
}

// 「本文|ファイル|印|未保存のタブの数|退避」。退避は合図 1 回で書いたら `stored`（前の合図が
// 同じ本文を書いていれば書かないので、1 つの場で合図を打つのはここだけ）。
[[nodiscard]] std::string observed(Editing &editing, std::string_view initial)
{
    auto &controller = editing.controller();
    const std::string body = whole_vim_body(controller);
    const std::string file = file_of(editing, initial);
    const bool saved = controller.frame().document.save_state == core::SaveState::saved;
    const std::size_t unsaved = app::unsaved_tab_count(controller.documents());
    const std::size_t stored = editing.autosave().stored().size();
    static_cast<void>(controller.apply(app::AutosaveTick{}));
    const bool kept = editing.autosave().stored().size() > stored;
    return body + "|" + file + "|" + (saved ? "saved" : "modified") + "|" +
           std::to_string(unsaved) + "|" + (kept ? "stored" : "none");
}

// ファイルと本文が違えば、未保存の印・閉じるときの問い・退避のどれも「未保存」の側にある。
// wanted が空でなければ行の全体も比べる。
void check(Editing &editing, std::string_view label, std::string_view wanted,
           std::string_view initial = original)
{
    const std::string body = whole_vim_body(editing.controller());
    const std::string file = file_of(editing, initial);
    const std::string row = observed(editing, initial);
    if (body != file)
    {
        expect(row.ends_with("|modified|1|stored"),
               (std::string(label) + ": a body that differs from the file stays unsaved: " + row)
                   .c_str());
    }
    if (!wanted.empty())
    {
        expect(row == wanted,
               (std::string(label) + ": " + row + " (wanted " + std::string(wanted) + ")").c_str());
    }
}

// 開いて録画まで済ませた編集の場。
void opened(Editing &editing, std::string_view keys)
{
    open_vim_document(editing, std::string(original));
    vim_replay(editing.controller(), keys);
}

// 1. レビューの反例と回数・位置の変形。`2@a` は `:w` の後に編集が残る周が 2 つ。
void verify_counted_macros()
{
    {
        Editing editing;
        opened(editing, "qa:w<CR>xqu2@a");
        check(editing, "qa:w<CR>xq u 2@a", "c\n|bc\n|modified|1|stored");
    }
    {
        Editing editing;
        opened(editing, "qa:w<CR>xqu3@a");
        check(editing, "qa:w<CR>xq u 3@a", "\n|c\n|modified|1|stored");
    }
    {
        // 1 回の `@a` で `:w` の後に編集が 1 つ（`:w` の前にも 1 つ）。
        Editing editing;
        opened(editing, "qax:w<CR>xquu@a");
        check(editing, "qax:w<CR>xq uu @a", "c\n|bc\n|modified|1|stored");
    }
    {
        // `:w` の後に編集が 2 つ。
        Editing editing;
        opened(editing, "qax:w<CR>xxquuu@a");
        check(editing, "qax:w<CR>xxq uuu @a", "\n|bc\n|modified|1|stored");
    }
}

// 2. `:w` が再生の最後（ファイルと本文が同じ）と最初（畳んだ単位を戻すとファイルと同じ）。
void verify_save_at_the_ends()
{
    {
        Editing editing;
        opened(editing, "qaxx:w<CR>quu@a");
        check(editing, "qaxx:w<CR>q uu @a", "c\n|c\n|saved|0|none");
        vim_replay(editing.controller(), "u");
        check(editing, "... u", "abc\n|c\n|modified|1|stored");
        vim_replay(editing.controller(), "<C-r>");
        check(editing, "... u <C-r>", "c\n|c\n|saved|0|none");
    }
    {
        Editing editing;
        opened(editing, "qa:w<CR>xxquu@a");
        check(editing, "qa:w<CR>xxq uu @a", "c\n|abc\n|modified|1|stored");
        vim_replay(editing.controller(), "u");
        check(editing, "... u", "abc\n|abc\n|saved|0|none");
    }
}

// 3. 畳んだ単位を `u` で戻した先と `<C-r>` の先（どちらもファイルは再生の途中の本文）。
void verify_undo_redo_after_fold()
{
    Editing editing;
    opened(editing, "qax:w<CR>xquu@a");
    vim_replay(editing.controller(), "u");
    check(editing, "x:w<CR>x then u", "abc\n|bc\n|modified|1|stored");
    vim_replay(editing.controller(), "<C-r>");
    check(editing, "x:w<CR>x then u <C-r>", "c\n|bc\n|modified|1|stored");
}

// 4. 差し引き 0 の再生・再生の中の undo・INSERT の `<C-o>:w`・map の右辺。
void verify_other_shapes()
{
    {
        // `:w` の後の編集が差し引き 0（`xu`）。
        Editing editing;
        opened(editing, "qax:w<CR>xuquu@a");
        check(editing, "x:w<CR>xu", "bc\n|bc\n|saved|0|none");
    }
    {
        // 再生の中で再生の前の単位を戻して別の編集を打ち、`:w` の後にもう 1 つ。保存時点の位置は
        // 再生の前の位置と同じ数だが、指す本文は再生の前の単位の本文ではない。
        Editing editing;
        opened(editing, "qau$x:w<CR>xxquuu0x");
        vim_replay(editing.controller(), "@a");
        check(editing, "u $x :w x inside @a", "\n|ab\n|modified|1|stored");
        vim_replay(editing.controller(), "u");
        check(editing, "u $x :w x inside @a, then u", "bc\n|ab\n|modified|1|stored");
    }
    {
        Editing editing;
        opened(editing, "qaiX<C-o>:w<CR>Y<Esc>qu@a");
        check(editing, "INSERT <C-o>:w inside @a", "XXYabc\n|XXabc\n|modified|1|stored");
    }
    {
        Editing editing;
        open_vim_document(editing, std::string(original));
        static_cast<void>(run_ex(editing.controller(), "nnoremap Q x:w<CR>x"));
        vim_replay(editing.controller(), "Q");
        check(editing, "map Q x:w<CR>x", "c\n|bc\n|modified|1|stored");
    }
    for (const std::string_view keys : {"x:wq\rx", "x:x\rx", "x:saveas copy.txt\rx"})
    {
        // 窓を閉じる・名前を替える保存（レジスタに直に置いて流す）。
        Editing editing;
        open_vim_document(editing, std::string(original));
        editing.files().resolve_to(core::FilePath::parse("C:\\other\\copy.txt").value());
        applied(editing.controller(),
                app::StoreVimRegister{
                    'a', core::VimRegister{std::string(keys), core::VimRegisterKind::characters}});
        vim_replay(editing.controller(), "x@a");
        check(editing, keys, "");
    }
    {
        // 名前の違う書き出し（保存時点は変えない）と :saveas の後の編集。
        Editing editing;
        opened(editing, "qax:w copy.txt<CR>xquu@a");
        check(editing, ":w copy.txt inside @a", "c\n|abc\n|modified|1|stored");
    }
    {
        // 再生の中でタブを替える（leave_document の畳み）。
        Editing editing;
        open_vim_document(editing, std::string(original));
        applied(editing.controller(), app::NewTab{});
        applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
        vim_replay(editing.controller(), "gt");
        vim_replay(editing.controller(), "qax:w<CR>xgtgtquu");
        vim_replay(editing.controller(), "@a");
        check(editing, "tab switch inside @a", "c\n|bc\n|modified|1|stored");
    }
}

// 文字コード・改行の形を替えた保存はファイルのバイト列が本文と違うので、行の全体だけを比べる。
void check_row(Editing &editing, std::string_view label, std::string_view wanted)
{
    const std::string row = observed(editing, original);
    expect(row == wanted,
           (std::string(label) + ": " + row + " (wanted " + std::string(wanted) + ")").c_str());
}

// 5. 保存の失敗（保存時点は動かない）と、文字コードだけ・改行の形だけを替えた再生の中の保存。
void verify_failed_and_format_saves()
{
    {
        Editing editing;
        opened(editing, "qax:w<CR>xquu");
        editing.files().refuse_writes(app::FileFailure::access_denied);
        vim_replay(editing.controller(), "@a");
        // 失敗した `:w` は再生を打ち切る（ADR 0022 の追補 #1526 の決定 6）。本文は録画の `:w`
        // が書いた ファイルと同じだが、保存時点の先を新しい編集が切ったので未保存のまま（安全側）。
        check(editing, "refused :w inside @a", "bc\n|bc\n|modified|1|stored");
    }
    {
        Editing editing;
        opened(editing, "qax:set fenc=utf-8-bom<CR>:w<CR>xquu@a");
        check_row(editing, "fenc then :w inside @a",
                  "c\n|\xEF\xBB\xBF"
                  "bc\n|modified|1|stored");
        vim_replay(editing.controller(), "u");
        check_row(editing, "fenc then :w inside @a, then u",
                  "abc\n|\xEF\xBB\xBF"
                  "bc\n|modified|1|stored");
    }
    {
        Editing editing;
        opened(editing, "qaxx:set fenc=utf-8-bom<CR>:w<CR>quu@a");
        check_row(editing, "fenc then :w last inside @a",
                  "c\n|\xEF\xBB\xBF"
                  "c\n|saved|0|none");
    }
    {
        Editing editing;
        opened(editing, "qax:set ff=dos<CR>:w<CR>xquu@a");
        check_row(editing, "ff then :w inside @a", "c\n|bc\r\n|modified|1|stored");
    }
    {
        Editing editing;
        opened(editing, "qaxx:set ff=dos<CR>:w<CR>quu@a");
        check_row(editing, "ff then :w last inside @a", "c\n|c\r\n|saved|0|none");
    }
}

constexpr std::string_view two_lines = "aa\nbb\n";

// Ctrl+S（窓の保存の操作）。Vim の INSERT でも入力行を通らず、矩形の INSERT の待ちは残る。
void save_now(Editing &editing)
{
    static_cast<void>(editing.controller().apply_frame(
        app::SaveDocument{sample_path(), core::TextEncoding::utf8}));
}

// 矩形の INSERT を打って Ctrl+S し、続き（after）の鍵を流す。
void block_saved(Editing &editing, std::string_view text, std::string_view typed,
                 std::string_view after)
{
    open_vim_document(editing, std::string(text));
    vim_replay(editing.controller(), typed);
    save_now(editing);
    vim_replay(editing.controller(), after);
}

// 6. 矩形の INSERT の Esc の畳み（fold_block_into_insert）。打った直後の Ctrl+S の後の Esc は、保存
// した単位を外して写しを足した単位に置き換える。
void verify_block_insert_folds()
{
    // 打つ鍵・Ctrl+S の後の鍵・行の全体（空なら性質だけ）。
    using Shape = std::array<std::string_view, 3>;
    for (const auto &[typed, after, wanted] : {
             Shape{"<C-v>jIX", "<Esc>", "Xaa\nXbb\n|Xaa\nbb\n|modified|1|stored"},
             Shape{"<C-v>jAX", "<Esc>", "aXa\nbXb\n|aXa\nbb\n|modified|1|stored"},
             Shape{"<C-v>j$AX", "<Esc>", "aaX\nbbX\n|aaX\nbb\n|modified|1|stored"},
             Shape{"<C-v>jcX", "<Esc>", "Xa\nXb\n|Xa\nb\n|modified|1|stored"},
             Shape{"<C-v>jCX", "<Esc>", "X\nX\n|X\n\n|modified|1|stored"},
             Shape{"<C-v>jIX", "Y<Esc>", "XYaa\nXYbb\n|Xaa\nbb\n|modified|1|stored"},
             Shape{"<C-v>jIX", "<BS><Esc>", ""},
             Shape{"<C-v>IX", "<Esc>", "Xaa\nbb\n|Xaa\nbb\n|saved|0|none"},
             Shape{"<C-v>jIX", "<Esc>u", "aa\nbb\n|Xaa\nbb\n|modified|1|stored"},
             Shape{"<C-v>jIX", "<Esc>u<C-r>", "Xaa\nXbb\n|Xaa\nbb\n|modified|1|stored"},
             Shape{"<C-v>jIX", "<Esc>.", "XXaa\nXXbb\n|Xaa\nbb\n|modified|1|stored"},
             Shape{"<C-v>jIX", "<C-o>:w<CR><Esc>", ""},
         })
    {
        Editing editing;
        block_saved(editing, two_lines, typed, after);
        check(editing, std::string(typed) + " Ctrl+S " + std::string(after), wanted, two_lines);
    }
    {
        Editing editing;
        block_saved(editing, "aa\nbb\ncc\n", "<C-v>2jIX", "<Esc>");
        check(editing, "<C-v>2jIX Ctrl+S <Esc>", "Xaa\nXbb\nXcc\n|Xaa\nbb\ncc\n|modified|1|stored",
              "aa\nbb\ncc\n");
    }
    {
        // 保存の失敗では保存時点が動かない。
        Editing editing;
        open_vim_document(editing, std::string(two_lines));
        editing.files().refuse_writes(app::FileFailure::access_denied);
        vim_replay(editing.controller(), "<C-v>jIX");
        save_now(editing);
        vim_replay(editing.controller(), "<Esc>");
        check(editing, "refused Ctrl+S in block insert", "Xaa\nXbb\n|aa\nbb\n|modified|1|stored",
              two_lines);
    }
}

// 7. 矩形の INSERT の畳みと再生の畳みが重なる形。
void verify_block_insert_in_replay()
{
    {
        Editing editing;
        open_vim_document(editing, std::string(two_lines));
        vim_replay(editing.controller(), "qa<C-v>jIX<Esc>qu@a");
        check(editing, "block insert inside @a", "Xaa\nXbb\n|aa\nbb\n|modified|1|stored",
              two_lines);
        save_now(editing);
        check(editing, "block insert inside @a, Ctrl+S", "Xaa\nXbb\n|Xaa\nXbb\n|saved|0|none",
              two_lines);
        vim_replay(editing.controller(), "u");
        check(editing, "... u", "aa\nbb\n|Xaa\nXbb\n|modified|1|stored", two_lines);
        vim_replay(editing.controller(), "<C-r>");
        check(editing, "... u <C-r>", "Xaa\nXbb\n|Xaa\nXbb\n|saved|0|none", two_lines);
    }
    {
        Editing editing;
        open_vim_document(editing, std::string(two_lines));
        vim_replay(editing.controller(), "qa:w<CR><C-v>jIX<Esc>qu@a");
        check(editing, ":w then block insert inside @a", "Xaa\nXbb\n|aa\nbb\n|modified|1|stored",
              two_lines);
        vim_replay(editing.controller(), "u");
        check(editing, ":w then block insert inside @a, then u", "aa\nbb\n|aa\nbb\n|saved|0|none",
              two_lines);
    }
    {
        Editing editing;
        open_vim_document(editing, std::string(two_lines));
        vim_replay(editing.controller(), "qa<C-v>jIX<C-o>:w<CR>Y<Esc>qu@a");
        check(editing, "block insert <C-o>:w inside @a", "", two_lines);
    }
}
} // namespace

void verify_fold_save_point_scope()
{
    verify_counted_macros();
    verify_save_at_the_ends();
    verify_undo_redo_after_fold();
    verify_other_shapes();
    verify_failed_and_format_saves();
    verify_block_insert_folds();
    verify_block_insert_in_replay();
}
} // namespace nenenib::tests
