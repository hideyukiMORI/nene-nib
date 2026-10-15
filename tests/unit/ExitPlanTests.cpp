// scope `--exit-plan` の単体テスト（ADR 0133 の決定 2・5）。窓の終わり方ごとの未保存の扱いの表
// exit_plan の全組み合わせと、描画を失って閉じたときの一覧と履歴（窓を閉じたときと同じ）。
#include "DocumentView.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EndSession.hpp"
#include "ExitCause.hpp"
#include "ExitPlan.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "OpenDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedHistory.hpp"
#include "ScriptedSession.hpp"
#include "SessionEnd.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "UnsavedTab.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::DocumentView;
using nenenib::application::EndSession;
using nenenib::application::exit_plan;
using nenenib::application::ExitCause;
using nenenib::application::ExitPlan;
using nenenib::application::InsertText;
using nenenib::application::OpenDocument;
using nenenib::application::SessionEnd;
using nenenib::core::FilePath;
using nenenib::core::SaveState;

// 経路 × 未保存の数 → 扱い。未保存が無ければどの経路もすぐ終わる。1 本と 3 本で数ではなく有無が
// 決めることを見る。
using PlanRow = std::tuple<ExitCause, std::size_t, ExitPlan>;
constexpr std::array<PlanRow, 9> plan_rows{{
    {ExitCause::user_close, 0, ExitPlan::leave_now},
    {ExitCause::user_close, 1, ExitPlan::confirm_each_tab},
    {ExitCause::user_close, 3, ExitPlan::confirm_each_tab},
    {ExitCause::rendering_lost, 0, ExitPlan::leave_now},
    {ExitCause::rendering_lost, 1, ExitPlan::confirm_each_tab_without_cancel},
    {ExitCause::rendering_lost, 3, ExitPlan::confirm_each_tab_without_cancel},
    {ExitCause::session_end, 0, ExitPlan::leave_now},
    {ExitCause::session_end, 1, ExitPlan::block_session_end},
    {ExitCause::session_end, 3, ExitPlan::block_session_end},
}};

void verify_plan_rows()
{
    for (const auto &[cause, unsaved, plan] : plan_rows)
    {
        const std::string description = "the exit plan follows the table for cause " +
                                        std::to_string(static_cast<int>(cause)) + " with " +
                                        std::to_string(unsaved) + " unsaved tabs";
        expect(exit_plan(cause, unsaved) == plan, description.c_str());
    }
}

[[nodiscard]] DocumentView tab_in(SaveState state)
{
    return DocumentView{nenenib::core::tab_title_for(std::nullopt, state), std::nullopt,
                        nenenib::core::TextEncoding::utf8, state, std::nullopt};
}

void verify_unsaved_count()
{
    expect(application::unsaved_tab_count({}) == 0, "no tabs have no unsaved tab");
    const std::vector<DocumentView> tabs{tab_in(SaveState::saved), tab_in(SaveState::modified),
                                         tab_in(SaveState::saved), tab_in(SaveState::modified)};
    expect(application::unsaved_tab_count(tabs) == 2, "every unsaved tab is counted once");
}

[[nodiscard]] OpenDocument open_at(std::string_view text)
{
    auto parsed = FilePath::parse(text);
    expect(parsed.has_value(), "the exit plan contract path parses");
    return OpenDocument{std::move(parsed).value()};
}

// a.txt と b.txt を開いて b に 1 文字足した窓を、reason で閉じる。
void end_two_tabs(Editing &editing, SessionEnd reason)
{
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"a\n"});
    editing.files().hold_at("C:\\work\\b.txt", Bytes{"b\n"});
    applied(editing.controller(), open_at("C:\\work\\a.txt"));
    applied(editing.controller(), open_at("C:\\work\\b.txt"));
    applied(editing.controller(), InsertText{"x"});
    applied(editing.controller(), EndSession{reason});
}

[[nodiscard]] std::vector<std::string> session_names(Editing &editing)
{
    std::vector<std::string> names;
    const auto &written = editing.session().written();
    if (written.has_value())
    {
        for (const auto &tab : written.value().tabs)
        {
            names.emplace_back(tab.path.file_name());
        }
        names.push_back(">" + std::to_string(written.value().active));
    }
    return names;
}

[[nodiscard]] std::vector<std::string> history_names(Editing &editing)
{
    std::vector<std::string> names;
    const auto &written = editing.history().written();
    if (written.has_value())
    {
        for (const auto &path : written.value().files)
        {
            names.emplace_back(path.file_name());
        }
    }
    return names;
}

// 描画を失って閉じた窓は、窓を閉じたときと同じ一覧と履歴を 1 回ずつ書く（ADR 0133 の決定 1）。
void verify_rendering_lost_session()
{
    Editing closed;
    end_two_tabs(closed, SessionEnd::window_closed);
    Editing lost;
    end_two_tabs(lost, SessionEnd::rendering_lost);
    expect(session_names(lost) == std::vector<std::string>{"a.txt", "b.txt", ">1"} &&
               session_names(lost) == session_names(closed),
           "a window that lost rendering remembers the same tabs as a closed window");
    expect(lost.session().writes() == 1 && closed.session().writes() == 1,
           "a window that lost rendering writes the list once");
    expect(history_names(lost) == history_names(closed) && history_names(lost).size() == 2,
           "a window that lost rendering records the same history as a closed window");
}
} // namespace

void verify_exit_plan_scope()
{
    verify_plan_rows();
    verify_unsaved_count();
    verify_rendering_lost_session();
}
} // namespace nenenib::tests
