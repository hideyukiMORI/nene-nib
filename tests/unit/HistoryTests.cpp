// scope `--history` の単体テスト（ADR 0042 決定 2・ADR 0060 の決定 8）。閉じたファイルの履歴を
// 書き換える純関数 history_recorded / history_forgotten と、履歴の替え玉の往復。
#include "FileHistory.hpp"
#include "FileHistoryEdit.hpp"
#include "FileHistoryFailure.hpp"
#include "FilePath.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedHistory.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <expected>
#include <format>
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
} // namespace

void verify_history_contracts()
{
    verify_recorded_order();
    verify_recorded_same_file();
    verify_recorded_limit();
    verify_forgotten();
    verify_scripted_round_trip();
}

void verify_history_scope()
{
    verify_history_contracts();
}
} // namespace nenenib::tests
