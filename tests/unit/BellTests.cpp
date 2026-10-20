// Issue #789 / ADR 0153。断りの音は入力行の確定とファイル操作の失敗だけが鳴らし、鍵の失敗と
// 情報の知らせは鳴らさない。替え玉 ScriptedBell の回数だけを見る（OS の音は鳴らさない）。
#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveTick.hpp"
#include "CommandText.hpp"
#include "Editing.hpp"
#include "FileFailure.hpp"
#include "FileHistory.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <string>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;

[[nodiscard]] std::string notice(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

void prepare_vim(Editing &editing)
{
    applied(editing.controller(), app::VisibleLines{vim_visible_lines});
    editing.files().hold(Bytes{"alpha\n"});
    applied(editing.controller(), app::OpenDocument{sample_path()});
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
}

// 決定 1 (a)・2: 入力行の確定の断りは 1 回鳴り、同じ報せでも鍵の失敗と情報の知らせは鳴らない。
void verify_input_line_refusals()
{
    Editing editing;
    prepare_vim(editing);
    auto &controller = editing.controller();
    const std::size_t start = editing.bell().rings();
    expect(start == 0, "starting and opening never ring");

    auto frame = run_ex(controller, "nosuchcommand");
    expect(notice(frame) == "Unknown command" && editing.bell().rings() == 1,
           "an unknown Ex command (Vim E492) rings once");

    frame = run_ex(controller, "noh");
    expect(editing.bell().rings() == 1, ":noh is not a refusal and does not ring");

    vim_replay(controller, "/zzz<CR>");
    expect(notice(controller.frame()).starts_with("E486") && editing.bell().rings() == 2,
           "a confirmed search without a match (E486) rings once");
    vim_replay(controller, "n");
    expect(notice(controller.frame()).starts_with("E486") && editing.bell().rings() == 2,
           "the same E486 from the n key does not ring");

    vim_replay(controller, "$l");
    expect(editing.bell().rings() == 2, "l at the end of the line does not ring");
    vim_replay(controller, "u");
    expect(editing.bell().rings() == 2, "u at the oldest change does not ring");
}

// 決定 1 (b): `:w` の失敗（E13）は 1 回鳴る。成功の「Written」は鳴らない。
void verify_write_refusals()
{
    Editing editing;
    prepare_vim(editing);
    auto &controller = editing.controller();
    editing.files().resolve_to(core::FilePath::parse("C:\\other\\next.txt").value());
    editing.files().refuse_writes(app::FileFailure::already_exists);
    auto frame = run_ex(controller, "w next.txt");
    expect(notice(frame) == "E13: File exists" && editing.bell().rings() == 1,
           ":w onto an existing file (E13) rings once");

    Editing written;
    prepare_vim(written);
    frame = run_ex(written.controller(), "w");
    expect(notice(frame) == "Written" && written.bell().rings() == 0, "a written file is quiet");
}

// 決定 1 (b): 一覧から開けなかったファイルと自動退避の失敗は 1 回鳴る。
void verify_file_refusals()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    app::FileHistory history;
    history.files.push_back(core::FilePath::parse("C:\\docs\\gone.txt").value());
    editing.history().serve(history);
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{"@gone"});
    const auto frame = controller.apply_frame(app::SubmitCommand{});
    expect(notice(frame) == "開けませんでした: gone.txt" && editing.bell().rings() == 1,
           "a listed file that cannot be opened rings once");

    Editing saving;
    auto &autosaving = saving.controller();
    applied(autosaving, app::VisibleLines{10});
    applied(autosaving, app::InsertText{"a"});
    applied(autosaving, app::AutosaveTick{});
    saving.autosave().fail(app::AutosaveId{0}, app::AutosaveFailure::unwritable);
    expect(notice(autosaving.apply_frame(app::WorkCompleted{})) == "自動退避できませんでした" &&
               saving.bell().rings() == 1,
           "a failed autosave rings once");
}
} // namespace

void verify_bell_contracts()
{
    verify_input_line_refusals();
    verify_write_refusals();
    verify_file_refusals();
}
} // namespace nenenib::tests
