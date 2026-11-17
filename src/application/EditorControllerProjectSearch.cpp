#include "EditorController.hpp"

#include "CommandChoice.hpp"
#include "CommandPalette.hpp"
#include "FileFolder.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteQuery.hpp"
#include "PaletteScope.hpp"
#include "PlaceCaret.hpp"
#include "ProjectHitRows.hpp"
#include "ProjectQuery.hpp"
#include "ProjectSearchRequest.hpp"
#include "ProjectSearchRules.hpp"
#include "SelectionAnchoring.hpp"
#include "UiMessage.hpp"
#include "VimMarkEdits.hpp"
#include "VimSearchNotice.hpp"
#include "VimSearchNoticeKind.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// プロジェクトの全文検索の群（ADR 0167 の決定 1〜5・ADR 0158 の群の分け方）。面の出どころ `!` の
// 候補・券・取消・当たりを開いた後のキャレット。読みと照合は worker の上の adapter（段 G3）。
namespace nenenib::application
{
namespace
{
// 開いている Ctrl+P の面（無ければ null）。
[[nodiscard]] const core::CommandPalette *open_palette_of(const EditorState &state)
{
    const auto &input = state.command_input();
    return input.has_value() ? std::get_if<core::CommandPalette>(&input.value()) : nullptr;
}

// 面が `!` の出どころなら、その検索語（入力の借用）。
[[nodiscard]] std::optional<std::string_view> searched_query(const core::CommandPalette *palette)
{
    if (palette == nullptr)
    {
        return std::nullopt;
    }
    const core::PaletteQuery query = core::palette_query_of(palette->input().text());
    if (query.scope != core::PaletteScope::project_hits)
    {
        return std::nullopt;
    }
    return query.query;
}

[[nodiscard]] bool finished(ProjectSearchPhase phase) noexcept
{
    switch (phase)
    {
    case ProjectSearchPhase::searching:
        return false;
    case ProjectSearchPhase::complete:
    case ProjectSearchPhase::truncated:
    case ProjectSearchPhase::failed:
        return true;
    }
    std::unreachable();
}

[[nodiscard]] ProjectSearchStatus status_of(ProjectSearchPhase phase) noexcept
{
    switch (phase)
    {
    case ProjectSearchPhase::searching:
        return ProjectSearchStatus::searching;
    case ProjectSearchPhase::complete:
        return ProjectSearchStatus::complete;
    case ProjectSearchPhase::truncated:
        return ProjectSearchStatus::truncated;
    case ProjectSearchPhase::failed:
        return ProjectSearchStatus::failed;
    }
    std::unreachable();
}
} // namespace

// 入口の 2 つ目（決定 1）。開いている入力行（検索の preview も）は取消と同じ道で閉じてから開く
// （OpenOperationList と同じ・録画に `<Esc>` を積まない）。`:grep` と同じ run_palette_request の 1
// 本 で、頼むのは open_palette の終わりの settle_project_search。
void EditorController::accept(const OpenProjectSearch &)
{
    if (state_.composition().has_value())
    {
        return;
    }
    if (command_line_active())
    {
        cancel_command_input();
    }
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::project_hits, {}});
}

// 1 打鍵ごとに意図の出口を通るので、面が閉じていて頼んでもいないときは欄を 3 つ読むだけで抜ける。
// 開いた当たりのキャレット（決定 5）も同じ出口で置く（開く道は裏で揃うことがある）。
void EditorController::settle_project_search()
{
    land_project_hit();
    if (!project_search_.pattern.has_value() && !command_palette_active())
    {
        return;
    }
    const auto query = searched_query(open_palette_of(state_));
    if (!query.has_value())
    {
        // 面が閉じた・ほかの出どころへ移った（決定 6: 面を閉じると cancel）。
        cancel_project_search();
        project_search_ = PaletteProjectSearch{};
        return;
    }
    if (project_search_.pattern.has_value() && project_search_.pattern.value() == query.value())
    {
        return;
    }
    start_project_search(query.value());
}

// 打つたびに新しい券で頼み、前の券を捨てる（300 ms の間を置かない・決定 1）。前の当たりは面の列から
// 外す（届いた順の列を検索語ごとに作り直す）。
void EditorController::start_project_search(std::string_view query)
{
    cancel_project_search();
    if (const auto *palette = open_palette_of(state_); palette != nullptr)
    {
        auto dropped = palette->dropped(core::PaletteOrigin::project_hit);
        state_ = state_.with_command_input(std::move(dropped));
    }
    project_search_ = PaletteProjectSearch{};
    project_search_.pattern = std::string(query);
    project_search_.ticket = ++project_tickets_;
    auto pattern = core::ProjectQuery::parse(query);
    if (!pattern)
    {
        project_search_.status = ProjectSearchStatus::invalid;
        project_search_.failure = pattern.error();
        return;
    }
    if (pattern.value().empty())
    {
        project_search_.status = ProjectSearchStatus::prompt;
        return;
    }
    const auto root = project_search_root();
    project_search_.root = root;
    if (!root.has_value())
    {
        project_search_.status = ProjectSearchStatus::unplaced;
        return;
    }
    project_search_.status = ProjectSearchStatus::searching;
    project_search_.live = true;
    project_draining_ = true;
    ports_.project_search.request(
        ProjectSearchRequest{root.value(), std::move(pattern).value(), project_search_.ticket});
}

void EditorController::cancel_project_search()
{
    if (!project_search_.live)
    {
        return;
    }
    project_search_.live = false;
    ports_.project_search.cancel(project_search_.ticket);
}

std::optional<core::FilePath> EditorController::project_search_root() const
{
    if (state_.settings().root.has_value())
    {
        return state_.settings().root;
    }
    const auto &path = state_.document().path;
    if (!path.has_value())
    {
        return std::nullopt;
    }
    return core::folder_of(path.value());
}

// 届いた分を全部受け取り、今の券の分だけを採る（古い券・面を閉じた後に届いた分は捨てる）。
// 頼んでいない間は port を呼ばない。
void EditorController::collect_project_search()
{
    if (!project_draining_)
    {
        return;
    }
    const auto batches = ports_.project_search.collect();
    if (batches.empty() && !project_search_.live)
    {
        project_draining_ = false;
        return;
    }
    std::vector<core::CommandChoice> more;
    for (const ProjectSearchBatch &batch : batches)
    {
        if (project_search_.live && batch.ticket == project_search_.ticket)
        {
            adopt_project_batch(batch, more);
        }
    }
    const auto *palette = open_palette_of(state_);
    if (!more.empty() && palette != nullptr)
    {
        auto extended = palette->extended(more);
        state_ = state_.with_command_input(std::move(extended));
    }
}

// 当たりは届いた順のまま足す（並べ替えない・ARC-003）。project_search_hit_limit に届いたら残りを
// 捨てて取り消し、打ち切りの状態にする（adapter が truncated を返す前に届いた分も同じ上限）。
void EditorController::adopt_project_batch(const ProjectSearchBatch &batch,
                                           std::vector<core::CommandChoice> &more)
{
    const auto root = project_search_.root;
    if (!root.has_value())
    {
        return;
    }
    for (const ProjectSearchHit &hit : batch.hits)
    {
        if (project_search_.hits.size() >= core::project_search_hit_limit)
        {
            break;
        }
        more.push_back(core::project_hit_choice(
            hit.file, core::project_hit_row(root.value(), hit.file, hit.hit)));
        project_search_.hits.push_back(hit);
    }
    project_search_.progress = batch.progress;
    project_search_.status = status_of(batch.progress.phase);
    if (finished(batch.progress.phase))
    {
        project_search_.live = false;
        return;
    }
    if (project_search_.hits.size() >= core::project_search_hit_limit)
    {
        cancel_project_search();
        project_search_.progress.phase = ProjectSearchPhase::truncated;
        project_search_.status = ProjectSearchStatus::truncated;
    }
}

// 面の当たりの候補の i 番目は project_search_.hits の i 番目（面の結果は `!` の候補を列の順のまま
// 全部出し、controller は届いた順に列の後ろへ足すだけ・CommandPalette::result_of）。
void EditorController::open_project_hit(std::size_t index)
{
    if (index >= project_search_.hits.size())
    {
        return;
    }
    const ProjectSearchHit hit = project_search_.hits.at(index);
    close_command_input();
    project_landing_ =
        ProjectHitLanding{hit.file, core::TextPosition{hit.hit.line, hit.hit.column}};
    open_listed(hit.file, core::PaletteOrigin::project_hit, OpenedAs::pinned);
    land_project_hit();
}

// 開く道は裏で揃うことがあるので、意図の出口でも呼ぶ（置く先が無ければ欄を 1 つ読むだけ）。
void EditorController::land_project_hit()
{
    if (!project_landing_.has_value())
    {
        return;
    }
    const ProjectHitLanding landing = project_landing_.value();
    const auto &path = state_.document().path;
    if (path.has_value() && ports_.files.same_file(path.value(), landing.file))
    {
        project_landing_.reset();
        // 飛ぶ前の位置を `''` に置く（Vim の setpcmark・`m'` と同じ 1 本）。
        const auto from = core::vim_mark_position_of(state_.text(), state_.selection().caret);
        auto marks = core::vim_mark_set(state_.vim_marks(), U'\'', from);
        if (marks.has_value())
        {
            state_ = state_.with_vim_marks(std::move(marks).value());
        }
        accept(PlaceCaret{landing.at, core::SelectionAnchoring::collapse});
        return;
    }
    // 開く要求が残っていなければ開けなかった（知らせは開く道が出した）。
    if (!std::ranges::any_of(state_.readings(), [&](const DocumentReading &reading)
                             { return ports_.files.same_file(reading.path, landing.file); }))
    {
        project_landing_.reset();
    }
}

std::optional<ProjectSearchView> EditorController::project_search_view() const
{
    if (!project_search_.pattern.has_value())
    {
        return std::nullopt;
    }
    const auto searched = std::to_string(project_search_.progress.searched);
    const auto found = std::to_string(project_search_.progress.found);
    const auto hits = std::to_string(project_search_.hits.size());
    const auto line = [&]() -> core::DisplayText
    {
        switch (project_search_.status)
        {
        case ProjectSearchStatus::prompt:
            return ui_notice(core::UiMessage::palette_project_prompt);
        case ProjectSearchStatus::searching:
            return ui_notice(core::UiMessage::palette_project_searching, {searched, found});
        case ProjectSearchStatus::complete:
            return ui_notice(core::UiMessage::palette_project_complete, {hits, searched});
        case ProjectSearchStatus::truncated:
            return ui_notice(core::UiMessage::palette_project_truncated, {hits});
        case ProjectSearchStatus::failed:
            return ui_notice(core::UiMessage::palette_project_failed);
        case ProjectSearchStatus::unplaced:
            return ui_notice(core::UiMessage::palette_project_no_root);
        case ProjectSearchStatus::invalid:
            // Vim の `/` の検索が解析できないときと同じ文の出どころ（ADR 0032 の決定 5）。
            return core::vim_search_message(core::VimSearchNotice{
                core::VimSearchNoticeKind::unsupported_pattern, {}, project_search_.failure});
        }
        std::unreachable();
    }();
    return ProjectSearchView{project_search_.status, project_search_.progress.searched,
                             project_search_.progress.found, project_search_.hits.size(), line};
}

std::vector<core::ProjectHitRow> EditorController::project_hit_rows(std::size_t first,
                                                                    std::size_t count) const
{
    std::vector<core::ProjectHitRow> rows;
    const auto &root = project_search_.root;
    if (!root.has_value() || first >= project_search_.hits.size())
    {
        return rows;
    }
    const std::size_t last = first + std::min(count, project_search_.hits.size() - first);
    rows.reserve(last - first);
    for (std::size_t index = first; index < last; ++index)
    {
        const ProjectSearchHit &hit = project_search_.hits.at(index);
        rows.push_back(core::project_hit_row(root.value(), hit.file, hit.hit));
    }
    return rows;
}
} // namespace nenenib::application
