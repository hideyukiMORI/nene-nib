// scope `--transient-tab-property` の単体テスト（#1397・ADR 0179 の決定 2・3・9）。固定 seed の
// 決定的な操作の列（面のクリック・面のダブルクリック・打鍵・保存・切り替え・閉じる・undo・
// 題名のダブルクリック・新しいタブ）を、エクスプローラーのクリックで開く controller（A）と、印を
// 持たない controller（B）の両方に流し、各歩で次を見る。
//   1. A の印のタブは高々 1 本で、その文書は保存済み（不変条件 2）。
//   2. A の帯（題名・パス・未保存の印）とアクティブと本文の先頭の行が、B と一致する。B は同じ列を
//      Ctrl+O の開く（pinned）で流し、A が置き換える場面だけを「印のタブへ切り替える → 開く →
//      印のタブを閉じる」の合成に読み替えたもの（決定 9）。
// 生成器は std::mt19937_64 の固定 seed で、分布の型は使わず剰余で範囲に畳む。
#include "CloseTab.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExplorerPointing.hpp"
#include "FilePath.hpp"
#include "FolderBatch.hpp"
#include "FolderEntry.hpp"
#include "FolderProgress.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "PinTab.hpp"
#include "PointExplorer.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSettings.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::ExplorerPointing;
using app::PointExplorer;
using Random = std::mt19937_64;

// 面の行 0〜4 は読めるファイル、行 5 は読めないファイル。
constexpr std::size_t readable_files = 5;
constexpr std::size_t panel_files = 6;
constexpr std::size_t cases = 40;
constexpr std::size_t steps = 60;
constexpr std::uint64_t first_seed = 13970U;

[[nodiscard]] std::string file_of(std::size_t row)
{
    return row < readable_files ? std::string("C:\\proj\\") + static_cast<char>('a' + row) + ".txt"
                                : std::string("C:\\proj\\missing.txt");
}

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] SettingsReading shown_project()
{
    auto settings = core::default_editor_settings();
    settings.explorer = true;
    settings.root = path_of("C:\\proj");
    return settings;
}

void load_project(Editing &editing)
{
    for (std::size_t row = 0; row < readable_files; ++row)
    {
        editing.files().hold_at(file_of(row), Bytes{"body " + std::to_string(row) + "\n"});
    }
    applied(editing.controller(), app::VisibleLines{10});
    app::FolderBatch batch{
        editing.folders().requests().back().ticket, {}, app::FolderProgress::complete};
    for (std::size_t row = 0; row < panel_files; ++row)
    {
        batch.entries.push_back(app::FolderEntry{path_of(file_of(row)), false, false});
    }
    editing.folders().serve(std::move(batch));
    applied(editing.controller(), app::WorkCompleted{});
}

void click(Editing &editing, std::size_t row)
{
    applied(editing.controller(), PointExplorer{row, ExplorerPointing::pressed});
    applied(editing.controller(), PointExplorer{row, ExplorerPointing::released});
}

void double_click(Editing &editing, std::size_t row)
{
    click(editing, row);
    applied(editing.controller(), PointExplorer{row, ExplorerPointing::double_click});
    applied(editing.controller(), PointExplorer{row, ExplorerPointing::released});
}

[[nodiscard]] std::optional<std::size_t> transient_of(const app::EditorFrame &frame)
{
    for (std::size_t index = 0; index < frame.tabs.size(); ++index)
    {
        if (frame.tabs.at(index).transient)
        {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool open_in(const app::EditorFrame &frame, std::size_t row)
{
    for (const auto &tab : frame.tabs)
    {
        if (tab.path.has_value() && tab.path.value() == path_of(file_of(row)))
        {
            return true;
        }
    }
    return false;
}

// 帯（題名・パス・未保存の印）とアクティブと本文の先頭の行。
[[nodiscard]] std::string printed(const app::EditorFrame &frame)
{
    std::string text;
    for (const auto &tab : frame.tabs)
    {
        text += std::string(tab.title.text()) + "(" +
                (tab.path.has_value() ? std::string(tab.path.value().text()) : "-") + ")" +
                (tab.save_state == core::SaveState::saved ? "" : "!") + "|";
    }
    text += "@" + std::to_string(frame.active_tab) + ":";
    text += frame.lines.empty() ? std::string("-") : frame.lines.front().text;
    return text;
}

// 1 印のタブは高々 1 本で保存済み（tab_views は印を 1 か所にしか立てないので、数えるのは本数）。
[[nodiscard]] bool marks_hold(const app::EditorFrame &frame)
{
    std::size_t count = 0;
    bool saved = true;
    for (const auto &tab : frame.tabs)
    {
        count += tab.transient ? 1U : 0U;
        saved = saved && (!tab.transient || tab.save_state == core::SaveState::saved);
    }
    return count <= 1 && saved;
}

// 面のクリックを B に写す。A が置き換える場面（読めるファイル・開いていない・印のタブがある）は
// 「印のタブへ切り替える → 開く → 印のタブを閉じる」の合成。ほかは Ctrl+O と同じ開く道。読めない
// ファイルは A でも何も変わらないので流さない。返すのは置き換えたか。
bool mirror_click(Editing &plain, const app::EditorFrame &before, std::size_t row)
{
    if (row >= readable_files)
    {
        return false;
    }
    const auto transient = transient_of(before);
    auto &controller = plain.controller();
    if (!open_in(before, row) && transient.has_value())
    {
        applied(controller, app::SwitchTab{transient.value()});
        applied(controller, app::OpenDocument{path_of(file_of(row))});
        applied(controller, app::CloseTab{transient.value()});
        return true;
    }
    applied(controller, app::OpenDocument{path_of(file_of(row))});
    return false;
}

void both(Editing &marked, Editing &plain, const app::EditorIntent &intent)
{
    applied(marked.controller(), intent);
    applied(plain.controller(), intent);
}

// 操作の 1 つを両方に流す。返すのは置き換えの数（0 か 1）。
std::size_t step(Editing &marked, Editing &plain, Random &random)
{
    const std::uint64_t kind = random() % 10U;
    const std::uint64_t value = random();
    const auto before = marked.controller().frame();
    const std::size_t tabs = before.tabs.size();
    switch (kind)
    {
    case 0:
    case 1:
    case 2:
        click(marked, value % panel_files);
        return mirror_click(plain, before, value % panel_files) ? 1U : 0U;
    case 3:
        double_click(marked, value % panel_files);
        return mirror_click(plain, before, value % panel_files) ? 1U : 0U;
    case 4:
        both(marked, plain, app::InsertText{"z"});
        return 0U;
    case 5:
        if (before.document.path.has_value())
        {
            both(marked, plain,
                 app::SaveDocument{before.document.path.value(), before.document.encoding});
        }
        return 0U;
    case 6:
        both(marked, plain, app::SwitchTab{value % tabs});
        return 0U;
    case 7:
        both(marked, plain, app::CloseTab{value % tabs});
        return 0U;
    case 8:
        both(marked, plain, app::HistoryAction{core::HistoryDirection::undo});
        return 0U;
    default:
        applied(marked.controller(), app::PinTab{value % tabs});
        return 0U;
    }
}

// 1 件ぶん。破れた歩の数を返し、replaced と marked_steps に置き換えと印のあった歩を足す。
std::size_t broken_case(std::uint64_t seed, std::size_t &replaced, std::size_t &marked_steps)
{
    Random random{seed};
    Editing marked(shown_project());
    Editing plain(shown_project());
    load_project(marked);
    load_project(plain);
    std::size_t broken = 0;
    for (std::size_t index = 0; index < steps; ++index)
    {
        replaced += step(marked, plain, random);
        const auto left = marked.controller().frame();
        const auto right = plain.controller().frame();
        marked_steps += transient_of(left).has_value() ? 1U : 0U;
        const bool holds = marks_hold(left) && printed(left) == printed(right);
        if (!holds && broken == 0)
        {
            std::fprintf(stderr, "transient-tab seed %llu step %zu:\n  A %s\n  B %s\n",
                         static_cast<unsigned long long>(seed), index, printed(left).c_str(),
                         printed(right).c_str());
        }
        broken += holds ? 0U : 1U;
    }
    return broken;
}
} // namespace

void verify_transient_tab_property_scope()
{
    std::size_t broken = 0;
    std::size_t replaced = 0;
    std::size_t marked_steps = 0;
    for (std::size_t index = 0; index < cases; ++index)
    {
        broken += broken_case(first_seed + index, replaced, marked_steps);
    }
    expect(
        broken == 0U,
        "after random clicks, edits, saves, switches, closes, undos and pins, at most one tab is "
        "transient and saved, and the tabs match the same steps with replacing read as closing "
        "and opening");
    expect(replaced > cases && marked_steps > cases * steps / 10U,
           "the generator replaces transient tabs and keeps a transient tab on many steps");
}
} // namespace nenenib::tests
