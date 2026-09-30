// scope `--history` の単体テスト（ADR 0042 決定 2・ADR 0060 の決定 8）。閉じたファイルの履歴を
// 書き換える純関数 history_recorded / history_forgotten と、履歴の替え玉の往復。
#include "Appearance.hpp"
#include "CloseTab.hpp"
#include "Column.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorPorts.hpp"
#include "EndSession.hpp"
#include "FileHistory.hpp"
#include "FileHistoryEdit.hpp"
#include "FileHistoryFailure.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedHistory.hpp"
#include "ScriptedSession.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedThemes.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionTab.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::FileHistory;
using nenenib::application::FileHistoryFailure;
using nenenib::application::history_forgotten;
using nenenib::application::history_limit;
using nenenib::application::history_recorded;
using nenenib::core::FilePath;

[[nodiscard]] FilePath path_of(std::string_view text)
{
    return FilePath::parse(text).value();
}

[[nodiscard]] FileHistory history_of(const std::vector<std::string_view> &texts)
{
    FileHistory history;
    for (const std::string_view text : texts)
    {
        history.files.push_back(path_of(text));
    }
    return history;
}

// 履歴のパスを `|` でつないだ文字列（順と中身を 1 回の比較で見る）。
[[nodiscard]] std::string joined(const FileHistory &history)
{
    std::string text;
    for (const FilePath &path : history.files)
    {
        if (!text.empty())
        {
            text += '|';
        }
        text += path.text();
    }
    return text;
}

// 新しい順に C:\0.txt … C:\{count-1}.txt。
[[nodiscard]] FileHistory numbered(std::size_t count)
{
    FileHistory history;
    for (std::size_t index = 0; index < count; ++index)
    {
        history.files.push_back(path_of(std::format("C:\\{}.txt", index)));
    }
    return history;
}

void verify_recorded_order()
{
    const ScriptedFiles files;
    const auto first = history_recorded(FileHistory{}, path_of("C:\\a.txt"), files);
    expect(joined(first) == "C:\\a.txt", "recording into an empty history leaves one file");
    const auto front =
        history_recorded(history_of({"C:\\a.txt", "C:\\b.txt"}), path_of("C:\\c.txt"), files);
    expect(joined(front) == "C:\\c.txt|C:\\a.txt|C:\\b.txt",
           "a new file goes to the front and the rest keep their order");
    const auto moved = history_recorded(history_of({"C:\\a.txt", "C:\\b.txt", "C:\\c.txt"}),
                                        path_of("C:\\b.txt"), files);
    expect(joined(moved) == "C:\\b.txt|C:\\a.txt|C:\\c.txt",
           "a file already in the history moves to the front once");
    const auto again =
        history_recorded(history_of({"C:\\a.txt", "C:\\b.txt"}), path_of("C:\\a.txt"), files);
    expect(joined(again) == "C:\\a.txt|C:\\b.txt", "recording the newest file changes nothing");
}

void verify_recorded_same_file()
{
    ScriptedFiles files;
    files.treat_as_same("C:\\Work\\A.txt", "C:\\work\\a.txt");
    const auto recorded = history_recorded(history_of({"C:\\b.txt", "C:\\work\\a.txt"}),
                                           path_of("C:\\Work\\A.txt"), files);
    expect(joined(recorded) == "C:\\Work\\A.txt|C:\\b.txt",
           "the same file in another case is kept once, under the recorded spelling");
    const auto doubled =
        history_recorded(history_of({"C:\\work\\a.txt", "C:\\b.txt", "C:\\Work\\A.txt"}),
                         path_of("C:\\c.txt"), files);
    expect(doubled.files.size() == 4,
           "recording another file does not merge entries it was not asked about");
}

void verify_recorded_limit()
{
    const ScriptedFiles files;
    const auto full = history_recorded(numbered(history_limit), path_of("C:\\new.txt"), files);
    expect(full.files.size() == history_limit, "a full history stays at the limit");
    expect(full.files.front().text() == "C:\\new.txt" && full.files.at(1).text() == "C:\\0.txt" &&
               full.files.back().text() == std::format("C:\\{}.txt", history_limit - 2),
           "the oldest file is cut and the order of the rest is kept");
    const auto over = history_recorded(numbered(history_limit + 5), path_of("C:\\x.txt"), files);
    expect(over.files.size() == history_limit, "a history longer than the limit is cut to it");
    const auto inside = history_recorded(numbered(history_limit), path_of("C:\\42.txt"), files);
    expect(inside.files.size() == history_limit && inside.files.front().text() == "C:\\42.txt" &&
               inside.files.back().text() == std::format("C:\\{}.txt", history_limit - 1),
           "moving a file inside a full history cuts nothing");
    expect(history_limit == 100, "the history keeps 100 files (ADR 0060 decision 8)");
}

void verify_forgotten()
{
    ScriptedFiles files;
    const auto removed = history_forgotten(history_of({"C:\\a.txt", "C:\\b.txt", "C:\\c.txt"}),
                                           path_of("C:\\b.txt"), files);
    expect(joined(removed) == "C:\\a.txt|C:\\c.txt", "a forgotten file leaves the history");
    const auto absent =
        history_forgotten(history_of({"C:\\a.txt", "C:\\c.txt"}), path_of("C:\\z.txt"), files);
    expect(joined(absent) == "C:\\a.txt|C:\\c.txt", "forgetting an absent file changes nothing");
    files.treat_as_same("C:\\A.txt", "C:\\a.txt");
    const auto cased = history_forgotten(history_of({"C:\\a.txt", "C:\\b.txt", "C:\\A.txt"}),
                                         path_of("C:\\A.txt"), files);
    expect(joined(cased) == "C:\\b.txt", "every spelling of the same file is forgotten");
    const auto emptied = history_forgotten(FileHistory{}, path_of("C:\\a.txt"), files);
    expect(emptied.files.empty(), "forgetting in an empty history is empty");
}

void verify_scripted_round_trip()
{
    ScriptedHistory history;
    const auto empty = history.read();
    expect(empty.has_value() && empty.value().files.empty(),
           "the stand-in reads an empty history by default");
    expect(history.write(history_of({"C:\\a.txt", "C:\\b.txt"})).has_value(),
           "the stand-in accepts a write");
    const auto back = history.read();
    expect(back.has_value() && joined(back.value()) == "C:\\a.txt|C:\\b.txt",
           "the next read returns what was written");
    expect(joined(history.written().value_or(FileHistory{})) == "C:\\a.txt|C:\\b.txt",
           "the stand-in remembers the written history");
    history.fail(FileHistoryFailure::unwritable);
    const auto refused = history.write(history_of({"C:\\c.txt"}));
    expect(!refused && refused.error() == FileHistoryFailure::unwritable,
           "a scripted write failure is returned");
    const auto kept = history.read();
    expect(kept.has_value() && joined(kept.value()) == "C:\\a.txt|C:\\b.txt",
           "a failed write leaves the previous history");
    expect(history.reads() == 3 && history.writes() == 2, "reads and writes are counted");
    ScriptedHistory broken{HistoryReading{std::unexpected(FileHistoryFailure::malformed)}};
    const auto failed = broken.read();
    expect(!failed && failed.error() == FileHistoryFailure::malformed,
           "a scripted read failure is returned");
}

// ---------------------------------------------------------------- 閉じたときに記録する（決定 8）

using nenenib::application::CloseTab;
using nenenib::application::EditorController;
using nenenib::application::EditorFrame;
using nenenib::application::EditorPorts;
using nenenib::application::EndSession;
using nenenib::application::InsertText;
using nenenib::application::NewTab;
using nenenib::application::OpenDocument;
using nenenib::application::SaveDocument;
using nenenib::application::Session;
using nenenib::application::SessionEnd;
using nenenib::application::SessionTab;
using nenenib::application::SwitchTab;
using nenenib::application::VisibleLines;

[[nodiscard]] OpenDocument work_open(char name)
{
    return OpenDocument{path_of("C:\\work\\" + std::string(1, name) + ".txt")};
}

// a.txt b.txt c.txt はどれも 1 行目がファイルの名前。
void hold_work(ScriptedFiles &files)
{
    for (const char name : {'a', 'b', 'c'})
    {
        files.hold_at("C:\\work\\" + std::string(1, name) + ".txt", Bytes{std::string(1, name)});
    }
}

void open_work(Editing &editing, std::string_view names)
{
    for (const char name : names)
    {
        applied(editing.controller(), work_open(name));
    }
}

// 最後に書いた履歴を `|` でつないで読む（読みの回数を増やさない）。
[[nodiscard]] std::string recorded(const ScriptedHistory &history)
{
    return joined(history.written().value_or(FileHistory{}));
}

[[nodiscard]] std::string notice_of(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

// パスのあるタブを閉じると、読んで先頭へ足して 1 回書く。無題と最後の 1 本は書かない。
void verify_close_records()
{
    Editing editing;
    hold_work(editing.files());
    applied(editing.controller(), VisibleLines{3});
    open_work(editing, "abc");
    editing.history().serve(history_of({"C:\\old.txt"}));
    applied(editing.controller(), CloseTab{0});
    expect(editing.history().reads() == 1 && editing.history().writes() == 1 &&
               recorded(editing.history()) == "C:\\work\\a.txt|C:\\old.txt",
           "closing a parked file tab records it in front of the history once");
    applied(editing.controller(), CloseTab{1});
    expect(editing.history().writes() == 2 &&
               recorded(editing.history()) == "C:\\work\\c.txt|C:\\work\\a.txt|C:\\old.txt" &&
               editing.controller().frame().tabs.size() == 1,
           "closing the active file tab records it the same way");
    applied(editing.controller(), CloseTab{0});
    expect(editing.history().writes() == 2 && editing.controller().frame().closing,
           "the last tab only asks to close the window and records nothing yet");
    Editing untitled;
    applied(untitled.controller(), InsertText{"x"});
    applied(untitled.controller(), NewTab{});
    applied(untitled.controller(), CloseTab{0});
    applied(untitled.controller(), CloseTab{0});
    expect(untitled.history().reads() == 0 && untitled.history().writes() == 0,
           "closing untitled tabs neither reads nor writes the history");
    Editing outside;
    applied(outside.controller(), CloseTab{5});
    expect(outside.history().reads() == 0 && outside.history().writes() == 0,
           "a tab outside the band does nothing");
}

[[nodiscard]] SessionReading three_listed()
{
    const auto tab = [](char name, std::size_t rank)
    {
        return SessionTab{
            path_of("C:\\work\\" + std::string(1, name) + ".txt"),
            nenenib::core::TextPosition{nenenib::core::LineNumber{1}, nenenib::core::Column{1}},
            nenenib::core::LineNumber{1}, rank};
    };
    return SessionReading{
        std::optional<Session>{Session{{tab('a', 1), tab('b', 0), tab('c', 2)}, 1}}};
}

// 窓を閉じると、パスのあるタブを使った順の古いほうから記録して、最後に見ていたタブが先頭になる。
// まだ読んでいないタブも入る。最後の 1 本を閉じたときはその 1 本（決定 8）。
void verify_end_session_records()
{
    Editing editing;
    hold_work(editing.files());
    applied(editing.controller(), VisibleLines{3});
    open_work(editing, "abc");
    applied(editing.controller(), SwitchTab{0});
    applied(editing.controller(), NewTab{});
    applied(editing.controller(), SwitchTab{0});
    applied(editing.controller(), EndSession{SessionEnd::window_closed});
    expect(editing.history().reads() == 1 && editing.history().writes() == 1 &&
               editing.session().writes() == 1 &&
               recorded(editing.history()) == "C:\\work\\a.txt|C:\\work\\c.txt|C:\\work\\b.txt",
           "closing the window records every file tab with the last viewed one first");
    Editing restored(three_listed(), hold_work);
    applied(restored.controller(), EndSession{SessionEnd::window_closed});
    expect(recorded(restored.history()) == "C:\\work\\b.txt|C:\\work\\a.txt|C:\\work\\c.txt" &&
               restored.files().reads() == 1,
           "tabs not read yet are recorded too, without reading them");
    Editing last;
    hold_work(last.files());
    open_work(last, "ab");
    applied(last.controller(), EndSession{SessionEnd::last_tab_closed});
    expect(last.history().writes() == 1 && recorded(last.history()) == "C:\\work\\b.txt",
           "closing the last tab records that one file");
    Editing blank;
    applied(blank.controller(), EndSession{SessionEnd::last_tab_closed});
    applied(blank.controller(), EndSession{SessionEnd::window_closed});
    expect(blank.history().reads() == 0 && blank.history().writes() == 0 &&
               blank.session().writes() == 2,
           "an untitled window writes its list but neither reads nor writes the history");
}

// 書けなくても読めなくても、状態も知らせも変わらない。読めなければ空の履歴から書く（決定 8）。
void verify_record_failures()
{
    Editing editing;
    hold_work(editing.files());
    applied(editing.controller(), VisibleLines{3});
    open_work(editing, "abc");
    editing.history().fail(FileHistoryFailure::unwritable);
    const EditorFrame closed = editing.controller().apply(CloseTab{2});
    expect(editing.history().writes() == 1 && !editing.history().written().has_value() &&
               closed.tabs.size() == 2 && closed.active_tab == 1 && notice_of(closed) == "none" &&
               !closed.document.last_failure.has_value(),
           "a failed history write changes neither the tabs nor the notices");
    const EditorFrame ended = editing.controller().apply(EndSession{SessionEnd::window_closed});
    expect(ended.tabs.size() == 2 && notice_of(ended) == "none" && editing.session().writes() == 1,
           "a failed history write at the end still writes the list once");
    editing.history().fail(std::nullopt);
    editing.history().serve(HistoryReading{std::unexpected(FileHistoryFailure::malformed)});
    applied(editing.controller(), CloseTab{0});
    expect(recorded(editing.history()) == "C:\\work\\a.txt",
           "an unreadable history is rewritten from the closed file");
}

// 起動・開く・切り替え・新しいタブ・保存・打鍵の道では、履歴を読まない・書かない（D24・決定 8）。
void verify_quiet_paths()
{
    {
        ScriptedAppearance appearance{Reading{Appearance::dark}};
        ScriptedClipboard clipboard;
        ScriptedFiles files;
        hold_work(files);
        ScriptedCodePages code_pages;
        ScriptedSettings settings;
        ScriptedThemes themes;
        ScriptedSession session;
        ScriptedHistory history;
        const EditorController controller(EditorPorts{appearance, clipboard, files, code_pages,
                                                      settings, themes, session, history},
                                          {work_open('a'), work_open('b')});
        expect(controller.frame().tabs.size() == 2 && history.reads() == 0 && history.writes() == 0,
               "starting with file arguments never touches the history");
    }
    Editing restored(three_listed(), hold_work);
    expect(restored.controller().frame().tabs.size() == 3 && restored.history().reads() == 0 &&
               restored.history().writes() == 0,
           "restoring the last tabs never touches the history");
    Editing editing;
    hold_work(editing.files());
    auto &controller = editing.controller();
    applied(controller, VisibleLines{3});
    open_work(editing, "ab");
    applied(controller, work_open('a'));
    applied(controller, SwitchTab{1});
    applied(controller, NewTab{});
    applied(controller, InsertText{"typed"});
    applied(controller,
            SaveDocument{path_of("C:\\work\\new.txt"), nenenib::core::TextEncoding::utf8});
    applied(controller, InsertText{"more"});
    expect(editing.history().reads() == 0 && editing.history().writes() == 0,
           "opening, switching, new tabs, saving and typing never touch the history");
}
} // namespace

void verify_history_contracts()
{
    verify_recorded_order();
    verify_recorded_same_file();
    verify_recorded_limit();
    verify_forgotten();
    verify_scripted_round_trip();
    verify_close_records();
    verify_end_session_records();
    verify_record_failures();
    verify_quiet_paths();
}

void verify_history_scope()
{
    verify_history_contracts();
}
} // namespace nenenib::tests
