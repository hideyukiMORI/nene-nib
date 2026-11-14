// scope `--replay-save-point` の単体テスト（#1672・ADR 0046 の追補）。再生（`@a`・`[count]@a`）は
// 終わりに積んだ編集を 1 つの undo の単位に畳む（merge_replayed_edits）。再生の中の `:w` は畳む前の
// 本文の位置で保存時点を覚えるので、写さないとファイルに無い本文を保存済みと判定した。替え玉の
// ファイルの port に書かれたバイト列と、本文・未保存の印・閉じるときに数える未保存のタブ
// （exit_plan に渡る数）・自動退避の合図が書くかを比べる。守る性質: ファイルと本文が違えば未保存。
#include "AutosaveTick.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedAutosave.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "StoreVimRegister.hpp"
#include "TestSupport.hpp"
#include "UnsavedTab.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimTestSupport.hpp"

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
[[nodiscard]] std::string file_of(Editing &editing)
{
    const auto &files = editing.files();
    const auto path = editing.controller().frame().document.path;
    if (path.has_value() && files.written_path() == path.value().text())
    {
        return files.written();
    }
    return std::string(original);
}

// 「本文|ファイル|印|未保存のタブの数|退避」。退避は合図 1 回で書いたら `stored`（前の合図が
// 同じ本文を書いていれば書かないので、1 つの場で合図を打つのはここだけ）。
[[nodiscard]] std::string observed(Editing &editing)
{
    auto &controller = editing.controller();
    const std::string body = whole_vim_body(controller);
    const std::string file = file_of(editing);
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
void check(Editing &editing, std::string_view label, std::string_view wanted)
{
    const std::string body = whole_vim_body(editing.controller());
    const std::string file = file_of(editing);
    const std::string row = observed(editing);
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
} // namespace

void verify_replay_save_point_scope()
{
    verify_counted_macros();
    verify_save_at_the_ends();
    verify_undo_redo_after_fold();
    verify_other_shapes();
}
} // namespace nenenib::tests
