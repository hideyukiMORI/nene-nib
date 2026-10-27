// scope `--ex-random` の単体テスト（Issue #911・ADR 0042 決定 2）。固定の種の生成器で Ex の
// 命令の名前と引数の断片を並べた `:` の行と、検索の行を作り、core の入口（evaluate_ex と
// CommandLine の編集）と controller の入口（VimKeyPress `:` / `/` / `?` → CommandText と
// EditCommand → SubmitCommand）に流して、壊れていないことを見る。Vim と同じかは見ない。
// 見るのは次の 5 つだけ（Issue #911 の受け入れ条件）。
//   1. 落ちない・止まらない（ASan / UBSan と CTest の TIMEOUT）
//   2. 結果は成功か閉じた失敗で、報せの文字列は空でない妥当な UTF-8（例外は出ない）
//   3. ファイルの替え玉への書き込みは、確定した行の名前が `write` `wq` `xit` `exit` `saveas`
//      の前方一致のときだけ 1 回まで。設定の替え玉への書き込みは名前が `colorscheme` か `set`
//      のときだけ 1 回まで。検索の行と取消はどちらも 0 回
//   4. 補完（Tab / Shift+Tab）と履歴の歩きをランダムな位置で押しても、入力行は妥当な UTF-8 で、
//      キャレットはコードポイントの境目、選んでいる候補は候補の列の中
//   5. 同じ入力で同じ結果（同じ試行を 2 回流して、本文・報せ・タブ・書き込みの数が一致）
// `:q` `:wq` `:x` は最後のタブで closing の印を立てるだけで、controller は窓もタブも閉じない
// （D22）ので語彙に残す。`"+` `"*` は語彙に入れない（CLAUDE.md・ADR 0051）。
// 生成器は std::mt19937_64 の固定の種で、分布の型は使わず剰余で範囲に畳む（決定的）。
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandLine.hpp"
#include "CommandText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorPorts.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExResult.hpp"
#include "FilePort.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedThemes.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::CommandEdit;
using Generator = std::mt19937_64;
// 入力行の 1 手。文字の断片を打つか、入力行の編集の鍵を 1 つ押すか。
using Step = std::variant<std::string, CommandEdit>;
// 1 行: 開く鍵（`:` `/` `?`）・手の列・確定するか（偽なら取消）。
using Line = std::tuple<char32_t, std::vector<Step>, bool>;
// 文書と行の列。破れは「どの不変量か」と「何行目か」。
using Trial = std::pair<std::string, std::vector<Line>>;
using Break = std::pair<std::string_view, std::size_t>;
// 1 回の試行の結果: 破れと、決定性を比べるための要約。
using Outcome = std::pair<std::optional<Break>, std::string>;

// 既定実行に載せる回数（Debug・ASan で 1 秒以内）。50 倍の走りは commit しない（Issue #911）。
constexpr std::size_t core_lines = 300;
constexpr std::size_t controller_trials = 4;
constexpr std::uint64_t random_seed = 911;
constexpr std::size_t longest_line = 200;

// ファイルの替え玉への書き込みを数える口（ScriptedFiles は最後の 1 回しか覚えない）。
class CountedFiles final : public app::FilePort
{
  public:
    using app::FilePort::write;

    explicit CountedFiles(ScriptedFiles &inner) noexcept : inner_(&inner) {}

    [[nodiscard]] std::size_t writes() const noexcept
    {
        return writes_;
    }

    [[nodiscard]] std::expected<SealedBytes, FileFailure> read(const FilePath &path,
                                                               std::size_t maximum_bytes) override
    {
        return inner_->read(path, maximum_bytes);
    }

    [[nodiscard]] std::expected<void, FileFailure> write(const FilePath &path,
                                                         std::span<const std::string_view> parts,
                                                         FileWriteMode mode) override
    {
        ++writes_;
        return inner_->write(path, parts, mode);
    }

    [[nodiscard]] std::expected<FilePath, FileFailure> resolve(const FilePath &path) override
    {
        return inner_->resolve(path);
    }

    [[nodiscard]] bool same_file(const FilePath &left, const FilePath &right) const override
    {
        return inner_->same_file(left, right);
    }

  private:
    ScriptedFiles *inner_;
    std::size_t writes_ = 0;
};

// 書き込みを数える 2 つの替え玉（ファイル・設定）。
using Counters = std::pair<const CountedFiles *, const ScriptedSettings *>;

[[nodiscard]] std::size_t below(Generator &generator, std::size_t bound)
{
    return static_cast<std::size_t>(generator() % bound);
}

template <std::size_t N>
[[nodiscard]] std::string pick(Generator &generator, const std::array<std::string_view, N> &table)
{
    return std::string(table.at(below(generator, N)));
}

// 命令の名前の材料（正しい形・省略・大文字の混ざり・Nib に無い Vim の名前）。
constexpr std::array<std::string_view, 44> names{
    "colorscheme", "colo", "set",     "se",         "syntax",   "syn",  "tabnext", "tabn",
    "tabprevious", "tabp", "tabNext", "tabnew",     "tabclose", "tabc", "tabs",    "tab",
    "write",       "w",    "wq",      "x",          "xit",      "exit", "saveas",  "sav",
    "quit",        "q",    "noh",     "nohlsearch", "only",     "on",   "edit",    "e",
    "buffer",      "b",    "ls",      "files",      "buffers",  "help", "normal",  "s",
    "g",           "r",    "",        "Write",
};

// 引数の断片（数・パス風・option・記号・空白・全角・絵文字）。
constexpr std::array<std::string_view, 64> arguments{
    "0",
    "1",
    "2",
    "99",
    "-1",
    "+1",
    "18446744073709551616",
    "999999999999999999999999",
    "a.txt",
    "C:\\x\\y.txt",
    "..\\up.txt",
    "foo bar.txt",
    "日本語.txt",
    "/tmp/x",
    "\\\\server\\share\\f",
    "C:",
    "fontsize=12",
    "fontsize=0",
    "fontsize=99999999999",
    "fontsize=",
    "guifont=Consolas",
    "guifont=",
    "guifont=游ゴシック",
    "wrap",
    "nowrap",
    "hlsearch",
    "noincsearch",
    "display=grid",
    "display=text",
    "display=",
    "filetype=cpp",
    "ft=zz",
    "ft?",
    "guide",
    "noguide",
    "all",
    "dracula",
    "solarized-light",
    "system",
    "SOL",
    "on",
    "off",
    "?",
    "!",
    "|",
    "=",
    "\"",
    "'",
    "/",
    " ",
    "   ",
    "\xe3\x80\x80",
    "全角",
    "😀",
    "👍🏽",
    "%",
    "#",
    "~",
    "*",
    "<",
    ">",
    "`",
    "$",
    "a\xcc\x81",
};

// 検索の行の断片（照合器の未対応構文・閉じない括弧・巨大な回数を含む）。
constexpr std::array<std::string_view, 30> pattern_pieces{
    "a", "x", "\\(", "\\)", "[",        "]",    "\\{", "*",   "\\",  "^",
    "$", ".", "\\<", "\\>", "\\|",      "日本", "😀",  "\\v", "\\%", "~",
    "/", "?", "\\/", " ",   "\\{9999}", "e+1",  ";",   "\\n", "\\_", "\xe3\x80\x80",
};

// 入力行の編集の鍵。補完と履歴は多めに引く（Issue #911 の 4）。
constexpr std::array<CommandEdit, 20> edits{
    CommandEdit::left,
    CommandEdit::right,
    CommandEdit::home,
    CommandEdit::end,
    CommandEdit::backspace,
    CommandEdit::erase,
    CommandEdit::complete_next,
    CommandEdit::complete_previous,
    CommandEdit::page_previous,
    CommandEdit::page_next,
    CommandEdit::list_first,
    CommandEdit::list_last,
    CommandEdit::history_previous,
    CommandEdit::history_next,
    CommandEdit::complete_next,
    CommandEdit::complete_next,
    CommandEdit::complete_previous,
    CommandEdit::complete_next,
    CommandEdit::history_previous,
    CommandEdit::left,
};

// 1 文字違い（置き換え・抜け・足し）の名前。
[[nodiscard]] std::string misspelled(Generator &generator, std::string name)
{
    constexpr std::string_view letters = "abcdeilnoqstwx!";
    const std::size_t kind = below(generator, 3);
    const char letter = letters.at(below(generator, letters.size()));
    if (name.empty() || kind == 2)
    {
        name.insert(name.begin() + static_cast<std::ptrdiff_t>(below(generator, name.size() + 1)),
                    letter);
        return name;
    }
    const std::size_t at = below(generator, name.size());
    if (kind == 0)
    {
        name.at(at) = letter;
        return name;
    }
    name.erase(at, 1);
    return name;
}

[[nodiscard]] std::string command_name(Generator &generator)
{
    std::string name = pick(generator, names);
    if (below(generator, 4) == 0)
    {
        name = misspelled(generator, name);
    }
    if (below(generator, 6) == 0)
    {
        name += "!";
    }
    const std::size_t lead = below(generator, 8);
    if (lead == 0)
    {
        return " " + name;
    }
    if (lead == 1)
    {
        return ":" + name;
    }
    if (lead == 2)
    {
        return std::to_string(below(generator, 12)) + name;
    }
    return name;
}

// 断片を打つ手の間に、ときどき編集の鍵を挟む。打つ文字は長さの上限で止める。
void push_pieces(Generator &generator, std::vector<Step> &steps, std::string text)
{
    while (!text.empty())
    {
        const std::size_t size = 1 + below(generator, text.size());
        steps.emplace_back(text.substr(0, size));
        text.erase(0, size);
        if (below(generator, 3) == 0)
        {
            steps.emplace_back(edits.at(below(generator, edits.size())));
        }
    }
}

// 正しい形の行。設定とファイルの替え玉へ書く道を試行の中で実際に通す（数える口の空振りを防ぐ）。
constexpr std::array<std::string_view, 18> well_formed{
    "colorscheme dracula",
    "colorscheme system",
    "set fontsize=14",
    "set guifont=Consolas",
    "set wrap",
    "set nowrap",
    "set noguide",
    "w",
    "w C:\\out\\a.txt",
    "w! C:\\out\\b.txt",
    "saveas C:\\out\\c.txt",
    "wq",
    "x",
    "tabnew",
    "tabnext 2",
    "tabclose",
    "syntax on",
    "noh",
};

[[nodiscard]] std::string ex_text(Generator &generator)
{
    const std::size_t kind = below(generator, 12);
    if (kind < 3)
    {
        return pick(generator, well_formed);
    }
    std::string text = kind == 3 ? std::string{} : command_name(generator);
    const std::size_t parts = below(generator, 5);
    for (std::size_t index = 0; index < parts; ++index)
    {
        const std::string separator = below(generator, 4) == 0 ? std::string{} : std::string{" "};
        const std::string next = text + separator + pick(generator, arguments);
        if (core::code_point_count(next) > longest_line)
        {
            break;
        }
        text = next;
    }
    return text;
}

[[nodiscard]] std::string search_text(Generator &generator)
{
    std::string text;
    const std::size_t parts = below(generator, 7);
    for (std::size_t index = 0; index < parts; ++index)
    {
        text += pick(generator, pattern_pieces);
    }
    return text;
}

[[nodiscard]] Line random_line(Generator &generator)
{
    const std::size_t kind = below(generator, 5);
    const char32_t prompt = kind == 0 ? U'/' : (kind == 1 ? U'?' : U':');
    const std::string text = prompt == U':' ? ex_text(generator) : search_text(generator);
    std::vector<Step> steps;
    push_pieces(generator, steps, text);
    if (below(generator, 3) == 0)
    {
        steps.emplace_back(edits.at(below(generator, edits.size())));
    }
    return Line{prompt, std::move(steps), below(generator, 6) != 0};
}

// 文書の材料。`:w` が書く本文が決まれば足りるので、数行の小さな文書だけ。
constexpr std::array<std::string_view, 5> documents{
    "", "abc def\nghi", "日本語のテキスト\r\nx\r\n", "aaa\nabc\naXa\n", "\tTab\there 😀\n",
};

[[nodiscard]] Trial random_trial(Generator &generator)
{
    Trial trial{pick(generator, documents), std::vector<Line>{}};
    const std::size_t lines = 10 + below(generator, 11);
    for (std::size_t index = 0; index < lines; ++index)
    {
        trial.second.push_back(random_line(generator));
    }
    return trial;
}

// 不変量 4: 入力行は妥当な UTF-8 で、キャレットは境目、候補の位置は列の中。
[[nodiscard]] bool line_sound(std::string_view text, std::size_t caret,
                              const std::vector<std::string> &completions,
                              std::optional<std::size_t> index)
{
    if (!core::validate_utf8(text).has_value() || caret > text.size())
    {
        return false;
    }
    if (caret < text.size() && (static_cast<unsigned char>(text.at(caret)) & 0xC0U) == 0x80U)
    {
        return false;
    }
    return !index.has_value() || index.value() < completions.size();
}

[[nodiscard]] bool message_sound(const std::optional<core::DisplayText> &message)
{
    return !message.has_value() || (!message.value().text().empty() &&
                                    core::validate_utf8(message.value().text()).has_value());
}

// 名前（先頭の空白と `:` を除いた英字の並び）が name の前方一致か。
[[nodiscard]] std::string leading_name(std::string_view text)
{
    const std::size_t start = text.find_first_not_of(" :");
    if (start == std::string_view::npos)
    {
        return {};
    }
    std::string name;
    for (const char letter : text.substr(start))
    {
        if ((letter < 'a' || letter > 'z') && (letter < 'A' || letter > 'Z'))
        {
            break;
        }
        name += letter;
    }
    return name;
}

// 不変量 3 の分類。過大に数える側（書いてよい行を広く取る）で、書いてはいけない行は 0 回。
[[nodiscard]] bool may_write_file(std::string_view text)
{
    const std::string name = leading_name(text);
    constexpr std::array<std::string_view, 5> writers{"write", "wq", "xit", "exit", "saveas"};
    for (const std::string_view writer : writers)
    {
        if (!name.empty() && writer.starts_with(name))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool may_write_settings(std::string_view text)
{
    const std::string name = leading_name(text);
    return name == "colorscheme" || name == "set";
}

// 1 つの核の不変量: Ex の入口は成功か閉じた失敗で、報せは空でない妥当な UTF-8。
[[nodiscard]] bool evaluated_soundly(std::string_view text)
{
    const auto result =
        core::evaluate_ex(text, core::default_editor_settings(), core::Appearance::dark);
    if (!result)
    {
        const auto message = core::ex_failure_message(result.error(), text);
        return !message.text().empty() && core::validate_utf8(message.text()).has_value();
    }
    return core::validate_utf8(result.value().message.text()).has_value();
}

// 核の CommandLine に手を流し、1 手ごとに不変量 4 を見る。確定する行は evaluate_ex にも渡す。
[[nodiscard]] std::optional<std::string_view> core_line_broken(const Line &line)
{
    auto command = core::CommandLine::empty();
    for (const Step &step : std::get<1>(line))
    {
        if (std::holds_alternative<CommandEdit>(step))
        {
            command = command.edited(std::get<CommandEdit>(step));
        }
        else
        {
            command = command.inserted(std::get<std::string>(step)).value_or(command);
        }
        if (!line_sound(command.text(), command.caret().value, command.completions(),
                        command.completion_index()))
        {
            return "the core command line stays valid UTF-8 on a boundary";
        }
    }
    if (!evaluated_soundly(command.text()))
    {
        return "evaluate_ex succeeds or fails closed with a message";
    }
    return std::nullopt;
}

void open_document(EditorController &controller, ScriptedFiles &files, std::string text)
{
    files.hold(Bytes{std::move(text)});
    static_cast<void>(controller.apply(app::VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(app::OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
}

void typed_steps(EditorController &controller, const std::vector<Step> &steps)
{
    for (const Step &step : steps)
    {
        if (std::holds_alternative<CommandEdit>(step))
        {
            static_cast<void>(controller.apply(app::EditCommand{std::get<CommandEdit>(step)}));
        }
        else
        {
            static_cast<void>(controller.apply(app::CommandText{std::get<std::string>(step)}));
        }
    }
}

[[nodiscard]] std::optional<std::string_view> frame_broken(const app::EditorFrame &frame,
                                                           const EditorController &controller)
{
    if (frame.command_line.has_value())
    {
        const auto &view = frame.command_line.value();
        if (!line_sound(view.text, view.caret.value, view.completions, view.completion_index))
        {
            return "the input line stays valid UTF-8 on a boundary";
        }
    }
    if (!message_sound(frame.command_message))
    {
        return "a message is non-empty valid UTF-8";
    }
    if (!core::validate_utf8(controller.text().text()).has_value())
    {
        return "the text stays valid UTF-8";
    }
    if (frame.tabs.empty() || frame.active_tab >= frame.tabs.size() ||
        frame.mode != core::EditMode::vim)
    {
        return "the tabs and the Vim mode stay whole";
    }
    return std::nullopt;
}

// 確定か取消を送り、書き込みの数を許しと比べる（不変量 3）。許しは確定した文字列から決める。
// 終わった後に開いたままの入力行（`:e` の面など）は取消で閉じる。
[[nodiscard]] std::optional<std::string_view> ended_broken(EditorController &controller,
                                                           const Line &line, std::string_view text,
                                                           Counters counters)
{
    const bool submitted = std::get<2>(line);
    const bool ex = std::get<0>(line) == U':' && submitted;
    const std::size_t file_writes = counters.first->writes();
    const std::size_t setting_writes = counters.second->writes();
    static_cast<void>(submitted ? controller.apply(app::SubmitCommand{})
                                : controller.apply(app::CancelCommand{}));
    if (counters.first->writes() - file_writes > (ex && may_write_file(text) ? 1U : 0U))
    {
        return "only a write command writes the file, once";
    }
    if (counters.second->writes() - setting_writes > (ex && may_write_settings(text) ? 1U : 0U))
    {
        return "only colorscheme or set writes the settings, once";
    }
    const auto after = frame_broken(controller.frame(), controller);
    if (controller.command_line_active() || controller.command_palette_active())
    {
        static_cast<void>(controller.apply(app::CancelCommand{}));
    }
    return after;
}

// 1 行を controller に流す。NORMAL に戻してから開く鍵を押し、手を打ってから終える。
[[nodiscard]] std::optional<std::string_view> line_broken(EditorController &controller,
                                                          const Line &line, Counters counters)
{
    vim_replay(controller, "<Esc>");
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{std::get<0>(line)}}));
    if (!controller.command_line_active())
    {
        return "the prompt opens the input line";
    }
    typed_steps(controller, std::get<1>(line));
    const auto typed = controller.frame();
    const auto broken = frame_broken(typed, controller);
    if (broken.has_value())
    {
        return broken;
    }
    const std::string text =
        typed.command_line.has_value() ? typed.command_line.value().text : std::string{};
    return ended_broken(controller, line, text, counters);
}

[[nodiscard]] std::string summary(EditorController &controller, const CountedFiles &files,
                                  const ScriptedSettings &settings)
{
    const auto frame = controller.frame();
    std::string text = controller.text().text() + "\x1f" + std::to_string(frame.tabs.size()) +
                       "\x1f" + std::to_string(frame.active_tab) + "\x1f" +
                       std::to_string(files.writes()) + "\x1f" + std::to_string(settings.writes());
    if (frame.command_message.has_value())
    {
        text += "\x1f" + std::string(frame.command_message.value().text());
    }
    return text;
}

// 破れの最初の 1 つと、決定性を比べる要約。替え玉の port だけを持つ controller を組む。
[[nodiscard]] Outcome outcome_of(const Trial &trial)
{
    Editing editing;
    CountedFiles files{editing.files()};
    ScriptedThemes themes;
    EditorController controller{
        app::EditorPorts{editing.appearance(), editing.clipboard(), files, editing.code_pages(),
                         editing.settings(), themes, editing.session(), editing.history(),
                         editing.folders(), editing.bookmarks(), editing.text_layouts(),
                         editing.autosave(), editing.fonts(), editing.bell(), editing.markdown()}};
    open_document(controller, editing.files(), trial.first);
    for (std::size_t index = 0; index < trial.second.size(); ++index)
    {
        const auto broken =
            line_broken(controller, trial.second.at(index), Counters{&files, &editing.settings()});
        if (broken.has_value())
        {
            return Outcome{Break{broken.value(), index}, std::string{}};
        }
    }
    return Outcome{std::nullopt, summary(controller, files, editing.settings())};
}

// 不変量 5 は同じ試行を 2 回流して比べる。破れの名前は試行の終わりの位置に置く。
[[nodiscard]] std::optional<Break> broken_in(const Trial &trial)
{
    const Outcome first = outcome_of(trial);
    if (first.first.has_value())
    {
        return first.first;
    }
    if (outcome_of(trial).second != first.second)
    {
        return Break{"the same input gives the same result", trial.second.size()};
    }
    return std::nullopt;
}

// 行を 1 つずつ外して、同じ不変量が破れる限り外したままにする（Issue #870 と同じ最小化）。
[[nodiscard]] Trial minimized(Trial trial, std::string_view invariant)
{
    for (std::size_t index = trial.second.size(); index > 0; --index)
    {
        Trial shorter = trial;
        shorter.second.erase(shorter.second.begin() + static_cast<std::ptrdiff_t>(index - 1));
        const auto broken = broken_in(shorter);
        if (broken.has_value() && broken.value().first == invariant)
        {
            trial = std::move(shorter);
        }
    }
    return trial;
}

[[nodiscard]] std::string described_line(const Line &line)
{
    std::string text = std::get<0>(line) == U':' ? ":" : (std::get<0>(line) == U'/' ? "/" : "?");
    for (const Step &step : std::get<1>(line))
    {
        text += std::holds_alternative<CommandEdit>(step)
                    ? "<edit " + std::to_string(static_cast<int>(std::get<CommandEdit>(step))) + ">"
                    : std::get<std::string>(step);
    }
    return text + (std::get<2>(line) ? "<CR>" : "<Esc>");
}

[[nodiscard]] std::string described(std::string_view label, const Trial &trial,
                                    std::string_view invariant)
{
    std::string lines;
    for (const Line &line : trial.second)
    {
        lines += described_line(line) + " ";
    }
    return std::string(label) + ": " + std::string(invariant) + " / lines " + lines;
}

void verify_core_lines()
{
    Generator generator{random_seed};
    for (std::size_t index = 0; index < core_lines; ++index)
    {
        const Line line = random_line(generator);
        const auto broken = core_line_broken(line);
        expect(!broken.has_value(),
               broken.has_value()
                   ? (std::string(broken.value()) + ": " + described_line(line)).c_str()
                   : "a random Ex line keeps the core invariants");
    }
}

void verify_controller_trials()
{
    Generator generator{random_seed + 1};
    for (std::size_t index = 0; index < controller_trials; ++index)
    {
        const Trial trial = random_trial(generator);
        const auto broken = broken_in(trial);
        if (!broken.has_value())
        {
            expect(true, "random Ex and search lines keep every invariant");
            continue;
        }
        const Trial smallest = minimized(trial, broken.value().first);
        expect(false,
               described("random trial " + std::to_string(index), smallest, broken.value().first)
                   .c_str());
    }
}

// 窓を閉じる Ex は最後のタブで closing の印を立てるだけで、タブも本文も残る（D22）。
void verify_closing_commands_only_mark()
{
    for (const std::string_view command : {"q", "quit!", "wq", "x", "exit"})
    {
        Editing editing;
        open_vim_document(editing, "abc");
        const auto delivery =
            editing.controller().apply(app::VimKeyPress{core::VimCharacter{U':'}});
        static_cast<void>(delivery);
        static_cast<void>(editing.controller().apply(app::CommandText{std::string(command)}));
        const auto submitted = editing.controller().apply(app::SubmitCommand{});
        const auto frame = editing.controller().frame();
        expect(submitted.closing && frame.tabs.size() == 1 && vim_body(frame) == "abc",
               "a closing Ex command only marks the last tab closing");
    }
}

// 1 回の取消と 1 回の確定の間で、空の行と空白だけの行は何も書かず閉じた結果で終わる。
void verify_blank_lines()
{
    for (const std::string_view text : {"", " ", "   ", ":", "\xe3\x80\x80", "|"})
    {
        Editing editing;
        open_vim_document(editing, "abc");
        const auto frame = run_ex(editing.controller(), std::string(text));
        expect(message_sound(frame.command_message) && !frame.command_line.has_value() &&
                   editing.files().written().empty() && editing.settings().writes() == 0,
               "a blank Ex line closes the input without writing");
    }
}

// 数える口が空振りしていない: 正しい形の `:set` と `:w` は 1 回ずつ書き、取消は書かない。
// 要約は「本文・タブの数・アクティブ・ファイルの書き込み・設定の書き込み」の順。
void verify_counters_see_writes()
{
    const Trial written{
        "abc",
        {Line{U':', {std::string{"set fontsize=14"}}, true}, Line{U':', {std::string{"w"}}, true}}};
    const Outcome counted = outcome_of(written);
    expect(!counted.first.has_value() && counted.second.starts_with("abc\x1f"
                                                                    "1\x1f"
                                                                    "0\x1f"
                                                                    "1\x1f"
                                                                    "1"),
           "a well-formed set and write each write once through the counted ports");
    const Trial cancelled{"abc",
                          {Line{U':', {std::string{"set fontsize=14"}}, false},
                           Line{U':', {std::string{"w"}}, false}}};
    const Outcome none = outcome_of(cancelled);
    expect(!none.first.has_value() && none.second == "abc\x1f"
                                                     "1\x1f"
                                                     "0\x1f"
                                                     "0\x1f"
                                                     "0",
           "cancelled lines write nothing");
}
} // namespace

void verify_ex_random_scope()
{
    verify_counters_see_writes();
    verify_closing_commands_only_mark();
    verify_blank_lines();
    verify_core_lines();
    verify_controller_trials();
}
} // namespace nenenib::tests
