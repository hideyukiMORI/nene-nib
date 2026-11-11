// scope `--controller-mixed-property` の単体テスト（#1288・ADR 0042 決定 2・ADR 0062 の決定 7・
// ADR 0137 の決定 1）。固定 seed の決定的な本文 3 つ（LF か CRLF・1 つは Markdown）と、編集・
// Vim の鍵・タブ（開く・切り替え・閉じる・歩き）・Ctrl+P の面（開く・打つ・選ぶ・閉じる）・Ex の
// 行・検索と置換の行・エクスプローラー・プレビュー・印刷・IME の変換の始まりと確定と取消を混ぜた
// 意図の列を EditorController に流し、各意図の後に次を見る。
//   1. frame() を作れて、layout の失敗が載らず、帯にタブが 1 本以上あり、アクティブが帯の中。
//   2. キャレットは本文の行の内（CRLF の対の間に無い）で、1 文字の途中（幅 0 の続き）に無く、
//      見えている行の選択の区間も行の内。frame の行数は本文の行数。
//   3. モードと待ちの状態は閉じた組: 面は入力行の 1 つ、「検索: 」は面ではない入力行、通常
//      モードの Vim は NORMAL で回数・オペレータ・1 鍵の待ちが無い、変換中なら変換の表示がある、
//      エクスプローラーの焦点は面が出ているときだけ。
//   4. 合図（WorkCompleted と AutosaveTick）を任意の位置に挟んだ列と挟まない列で、使う人の
//      各意図の後の観測（本文・キャレット・選択・帯・モード・Vim の待ち・入力行・面・知らせ）が
//      同じ（CLAUDE.md §2「合図は使う人の操作ではない」）。MoveCaret の直前にも WorkCompleted を
//      挟む（#1296 で絞りを外した・既知の組に最小の再現）。
//   5. 列の終わりに入力行・変換・焦点を閉じて通常モードへ戻し、帯の各タブで undo を歩の数より
//      多く打つと、そのタブが開いたときの本文（ファイルの本文・無題は空）に戻る。
// port は替え玉だけ（ファイル・クリップボード・時計・印刷・LSP・フォルダに触れない）。フォルダの
// 列挙は仕込まないので、WorkCompleted が受け取る結果は無い。
// Vim の鍵は窓と同じに状態で振り分ける（入力行が開いていれば CommandText / SubmitCommand /
// CancelCommand、エクスプローラーに焦点があれば ExplorerKey、通常モードなら InsertText などの
// 通常の意図、Vim なら VimKeyPress）。IME は窓と同じに変換の始まりから確定か取消までを続けて
// 送る（合図はその間にも挟む）。
#include "CaretMotion.hpp"
#include "ClauseEmphasis.hpp"
#include "ClipboardOperation.hpp"
#include "CommandEdit.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "DeleteDirection.hpp"
#include "DisplayWidth.hpp"
#include "DocumentView.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "ExplorerKeyKind.hpp"
#include "FilePath.hpp"
#include "HistoryDirection.hpp"
#include "LineEnding.hpp"
#include "LineView.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "PrintFailure.hpp"
#include "PrintPaper.hpp"
#include "PrintSettings.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimMode.hpp"
#include "VimSearchDirection.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <iterator>
#include <optional>
#include <random>
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

using Random = std::mt19937_64;
// 打った鍵の記法（vim_keys_of の 1 鍵）。状態で振り分けてから送る。
using Typed = std::string;
using Action = std::variant<app::EditorIntent, Typed>;
// 1 歩: 報告に出す名前と動作。
using Step = std::pair<std::string, Action>;
// 1 組: 3 つのファイルの本文と歩の列。
using Bodies = std::array<std::string, 3>;
using Case = std::pair<Bodies, std::vector<Step>>;
using Reason = std::string_view;
// 走りの結果: 最初に破れた不変条件（空なら無し）と、使う人の各歩の後の観測の列。
using Outcome = std::pair<Reason, std::vector<std::string>>;
using Check = Reason (*)(const Case &);
// 模型の 1 行: 始まり・内容の終わり。
using Line = std::pair<std::size_t, std::size_t>;

// Debug で 1 秒前後に収まる回数（#1288）。50 倍の走りは rounds を 50 にして手元で回し、commit
// しない。
constexpr std::size_t rounds = 1U;
constexpr std::size_t family_cases = 5U * rounds;
constexpr std::size_t step_floor = 30U;
constexpr std::size_t step_spread = 41U;
constexpr std::size_t visible_lines = 400U;
constexpr std::size_t tab_reach = 6U;
// 本番の組の合図を挟んだ列に、種類ごとに少なくとも出る回数（#1296）。
constexpr std::size_t signal_floor = 5U;

constexpr std::array<std::string_view, 3> paths{"C:\\m\\a.txt", "C:\\m\\b.txt", "C:\\m\\c.md"};
constexpr std::array<std::string_view, 12> body_tokens{
    "ab", "Hello", "x", " ", "\t", "日本語", "かな", "e\u0301", "# h", "- [ ] t", "a.b", "beta"};
// 1 鍵の記法。Vim の鍵と、通常モードで文字になる鍵の両方。
constexpr std::array<std::string_view, 40> keys{
    "i", "a", "o", "A", "x",  "X", "j",     "k",    "h",    "l",     "w",     "b",    "e", "0",
    "$", "d", "y", "c", "p",  "P", "u",     "v",    "V",    ".",     "g",     "t",    "n", "*",
    "r", "J", "G", "Z", "あ", " ", "<Esc>", "<CR>", "<BS>", "<C-r>", "<C-v>", "<Tab>"};
constexpr std::array<std::string_view, 12> ex_commands{
    "noh",      "tabnext", "tabprevious", "tabnew",   "tabclose",   "s/a/X/g",
    "%s/b/YY/", "2",       "set ic",      "set noic", "syntax off", "set nu"};
constexpr std::array<std::string_view, 10> palette_texts{"#", "@", "/", ":", "?",
                                                         "a", "0", "m", "*", "col"};
constexpr std::array<std::string_view, 6> find_texts{"a", "ab", "x", "2", "beta", "は"};
constexpr std::array<std::string_view, 6> typed_texts{"a", "Z", " ", "あ", "\t", "語"};
constexpr std::array<std::string_view, 4> kana{"に", "ほ", "ん", "ご"};
constexpr std::array<core::CaretMotion, 8> motions{
    core::CaretMotion::previous_character, core::CaretMotion::next_character,
    core::CaretMotion::previous_line,      core::CaretMotion::next_line,
    core::CaretMotion::line_start,         core::CaretMotion::line_end,
    core::CaretMotion::document_start,     core::CaretMotion::document_end};
constexpr std::array<app::ExplorerKeyKind, 6> explorer_keys{
    app::ExplorerKeyKind::j, app::ExplorerKeyKind::k,     app::ExplorerKeyKind::h,
    app::ExplorerKeyKind::l, app::ExplorerKeyKind::enter, app::ExplorerKeyKind::esc};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] core::FilePath path_at(std::size_t index)
{
    auto parsed = core::FilePath::parse(paths.at(index));
    return std::move(parsed).value();
}

// ---------------------------------------------------------------- 素朴な行と 1 文字（不変条件 2）

[[nodiscard]] bool lead_byte(char unit) noexcept
{
    return (static_cast<unsigned char>(unit) & 0xC0U) != 0x80U;
}

[[nodiscard]] std::vector<Line> lines_of(std::string_view text, bool crlf)
{
    std::vector<Line> lines;
    std::size_t begin = 0;
    for (std::size_t at = text.find('\n'); at != std::string_view::npos;
         at = text.find('\n', begin))
    {
        const bool paired = crlf && at > begin && text[at - 1U] == '\r';
        lines.emplace_back(begin, paired ? at - 1U : at);
        begin = at + 1U;
    }
    const bool stripped = crlf && text.size() > begin && text.back() == '\r';
    lines.emplace_back(begin, stripped ? text.size() - 1U : text.size());
    return lines;
}

[[nodiscard]] std::size_t points_in(std::string_view text, const Line &line)
{
    return static_cast<std::size_t>(
        std::ranges::count_if(text.substr(line.first, line.second - line.first), lead_byte));
}

// 行の内容の n 番目のコードポイントの位置（n が数と同じなら内容の終わり）。
[[nodiscard]] std::size_t nth_point(std::string_view text, const Line &line, std::size_t n)
{
    std::size_t at = line.first;
    for (std::size_t seen = 0; seen < n && at < line.second; ++seen)
    {
        ++at;
        while (at < line.second && !lead_byte(text[at]))
        {
            ++at;
        }
    }
    return at;
}

// at から始まるコードポイント（正しい UTF-8 だけを読む）。
[[nodiscard]] char32_t point_at(std::string_view text, std::size_t at)
{
    const auto first = static_cast<unsigned char>(text[at]);
    const std::size_t length = first < 0x80U ? 1U : first < 0xE0U ? 2U : first < 0xF0U ? 3U : 4U;
    constexpr std::array<unsigned, 5> masks{0x00U, 0x7FU, 0x1FU, 0x0FU, 0x07U};
    char32_t value = first & masks.at(length);
    for (std::size_t index = at + 1U; index < at + length && index < text.size(); ++index)
    {
        value = (value << 6U) | (static_cast<unsigned char>(text[index]) & 0x3FU);
    }
    return value;
}

// 位置 at が 1 文字の途中（手前の 1 文字に続く幅 0 の結合文字の前）か。手前の判定は簡略で、
// 本文の語に VS と制御文字の前の幅 0 は無い。
[[nodiscard]] bool inside_character(std::string_view text, std::size_t at)
{
    return at > 0U && at < text.size() &&
           core::display_width(point_at(text, at)) == core::DisplayWidth::zero &&
           text[at - 1U] != '\n' && text[at - 1U] != '\t';
}

[[nodiscard]] Reason span_violation(std::string_view text, const std::vector<Line> &lines,
                                    const app::LineView &line)
{
    if (line.selection.presence == core::SelectionPresence::absent)
    {
        return {};
    }
    const std::size_t index = line.number.value - 1U;
    if (line.number.value == 0U || index >= lines.size())
    {
        return "2: a selection span is on a line the text does not have";
    }
    const std::size_t limit = points_in(text, lines.at(index)) + 2U;
    return line.selection.begin.value == 0U || line.selection.end.value > limit ||
                   line.selection.begin.value > line.selection.end.value
               ? "2: a selection span is outside its line"
               : Reason{};
}

[[nodiscard]] Reason caret_violation(std::string_view text, const std::vector<Line> &lines,
                                     const app::EditorFrame &frame)
{
    const auto position = frame.caret.position;
    if (position.line.value == 0U || position.line.value > lines.size() ||
        position.column.value == 0U)
    {
        return "2: the caret is on a line the text does not have";
    }
    const Line &line = lines.at(position.line.value - 1U);
    const std::size_t count = points_in(text, line);
    if (position.column.value - 1U > count + 1U)
    {
        return "2: the caret is past the end of its line";
    }
    const bool past = position.column.value - 1U == count + 1U;
    const bool last = position.line.value == lines.size();
    if (past && !(last && line.second < text.size()))
    {
        return "2: the caret is inside a CRLF pair";
    }
    const std::size_t at = past ? text.size() : nth_point(text, line, position.column.value - 1U);
    return inside_character(text, at) ? "2: the caret is inside a character" : Reason{};
}

[[nodiscard]] Reason text_violation(app::EditorController &controller,
                                    const app::EditorFrame &frame)
{
    const std::string text = controller.text().text();
    const bool crlf = controller.text().line_ending() == core::LineEnding::crlf;
    const auto lines = lines_of(text, crlf);
    if (frame.total_lines != lines.size())
    {
        return "2: the frame does not count the lines of the text";
    }
    if (controller.is_composing())
    {
        return {};
    }
    Reason reason = caret_violation(text, lines, frame);
    for (const auto &line : frame.lines)
    {
        reason = reason.empty() ? span_violation(text, lines, line) : reason;
    }
    return reason;
}

// ---------------------------------------------------------------- 不変条件 1・3

[[nodiscard]] Reason band_violation(const app::EditorFrame &frame,
                                    const app::EditorController &controller)
{
    if (frame.layout_failure.has_value())
    {
        return "1: a layout failure is on the frame";
    }
    if (frame.tabs.empty() || frame.active_tab >= frame.tabs.size())
    {
        return "1: the active tab is outside the band";
    }
    return controller.documents().size() == frame.tabs.size()
               ? Reason{}
               : "1: documents() and the frame disagree on the tab count";
}

[[nodiscard]] Reason input_violation(const app::EditorFrame &frame,
                                     const app::EditorController &controller)
{
    const bool line = controller.command_line_active();
    const bool palette = controller.command_palette_active();
    if (palette && !line)
    {
        return "3: the palette is open without an input line";
    }
    if (controller.find_line_active() && (!line || palette))
    {
        return "3: the find line is open as the palette or without an input line";
    }
    if (frame.command_palette.has_value() != palette)
    {
        return "3: the frame shows the palette differently from command_palette_active";
    }
    return controller.explorer_focused() && !frame.explorer.has_value()
               ? "3: the explorer has the focus without its panel"
               : Reason{};
}

[[nodiscard]] Reason mode_violation(const app::EditorFrame &frame,
                                    const app::EditorController &controller)
{
    const auto &vim = controller.vim_state();
    if (frame.mode == core::EditMode::ordinary &&
        (vim.mode != core::VimMode::normal || vim.count.has_value() || vim.pending.has_value() ||
         vim.input_wait.has_value()))
    {
        return "3: the ordinary mode keeps a Vim mode, a count, an operator or a wait";
    }
    if (frame.vim_mode != vim.mode)
    {
        return "3: the frame shows another Vim mode";
    }
    return controller.is_composing() && !frame.composition.has_value() &&
                   !frame.command_composition.has_value()
               ? "3: a composition is not shown"
               : Reason{};
}

[[nodiscard]] Reason frame_violation(app::EditorController &controller, app::EditorFrame &frame)
{
    frame = controller.frame();
    Reason reason = band_violation(frame, controller);
    reason = reason.empty() ? input_violation(frame, controller) : reason;
    reason = reason.empty() ? mode_violation(frame, controller) : reason;
    return reason.empty() ? text_violation(controller, frame) : reason;
}

// ---------------------------------------------------------------- 観測（不変条件 4）

void append_number(std::string &out, std::size_t value)
{
    out += std::to_string(value);
    out += ',';
}

void append_vim(std::string &out, const core::VimState &vim)
{
    append_number(out, static_cast<std::size_t>(vim.mode));
    for (const bool held :
         {vim.count.has_value(), vim.pending.has_value(), vim.input_wait.has_value(),
          vim.selected_register.has_value(), vim.recording.has_value(), vim.last_change.has_value(),
          vim.macro_recording.has_value(), vim.wanted_column.has_value(),
          vim.insert_start.has_value(), vim.last_search.has_value()})
    {
        out += held ? '1' : '0';
    }
    out += '|';
}

void append_inputs(std::string &out, const app::EditorFrame &frame)
{
    if (frame.command_line.has_value())
    {
        const auto &line = frame.command_line.value();
        append_number(out, static_cast<std::size_t>(line.prompt));
        out += line.text + "|" + std::to_string(line.caret.value) + "|";
    }
    out += frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("-");
    out += frame.command_palette.has_value() ? "|P|" : "|-|";
    out += frame.explorer.has_value() ? (frame.explorer.value().focused ? "E1|" : "E0|") : "-|";
    if (frame.composition.has_value())
    {
        out += frame.composition.value().utf8 + "@" +
               std::to_string(frame.composition.value().cursor.value) + "|";
    }
}

// 最後のタブを閉じる印 closing・close_request・operation_request は「1 意図だけ」の印で、窓は
// その意図の配達で読む（ADR 0056 の決定 6・ADR 0078 の決定 8）。次の意図は合図でも消すので
// 観測に入れない（50 倍の走りで合図の直後に closing が消える組を見て、begin_intent
// を読んで確かめた）。
void append_band(std::string &out, const app::EditorFrame &frame)
{
    for (const auto &tab : frame.tabs)
    {
        out += std::string(tab.title.text()) + "/" +
               std::to_string(static_cast<int>(tab.save_state)) + ";";
    }
    append_number(out, frame.active_tab);
}

[[nodiscard]] std::string observation(app::EditorController &controller,
                                      const app::EditorFrame &frame)
{
    std::string out = controller.text().text() + "|";
    append_number(out, frame.caret.position.line.value);
    append_number(out, frame.caret.position.column.value);
    append_number(out, frame.first_visible.value);
    for (const auto &line : frame.lines)
    {
        if (line.selection.presence != core::SelectionPresence::absent)
        {
            append_number(out, line.number.value);
            append_number(out, line.selection.begin.value);
            append_number(out, line.selection.end.value);
        }
    }
    append_number(out, static_cast<std::size_t>(frame.mode));
    append_vim(out, controller.vim_state());
    append_inputs(out, frame);
    append_band(out, frame);
    out += controller.tab_walking() ? "W" : "-";
    return out;
}

// ---------------------------------------------------------------- 鍵の振り分け（窓の写し）

[[nodiscard]] std::optional<app::EditorIntent> line_key(std::string_view key)
{
    if (key == "<Esc>")
    {
        return app::CancelCommand{};
    }
    if (key == "<CR>")
    {
        return app::SubmitCommand{};
    }
    if (key == "<BS>")
    {
        return app::EditCommand{core::CommandEdit::backspace};
    }
    if (key == "<Tab>")
    {
        return app::EditCommand{core::CommandEdit::complete_next};
    }
    return key.starts_with('<')
               ? std::nullopt
               : std::optional<app::EditorIntent>{app::CommandText{std::string(key)}};
}

[[nodiscard]] std::optional<app::EditorIntent> explorer_key(std::string_view key)
{
    constexpr std::array<std::pair<std::string_view, app::ExplorerKeyKind>, 6> table{{
        {"j", app::ExplorerKeyKind::j},
        {"k", app::ExplorerKeyKind::k},
        {"h", app::ExplorerKeyKind::h},
        {"l", app::ExplorerKeyKind::l},
        {"<CR>", app::ExplorerKeyKind::enter},
        {"<Esc>", app::ExplorerKeyKind::esc},
    }};
    const auto found =
        std::ranges::find(table, key, &std::pair<std::string_view, app::ExplorerKeyKind>::first);
    return found == table.end() ? std::nullopt
                                : std::optional<app::EditorIntent>{app::ExplorerKey{found->second}};
}

[[nodiscard]] std::optional<app::EditorIntent> ordinary_key(std::string_view key)
{
    if (key == "<Esc>")
    {
        return app::CancelSelection{};
    }
    if (key == "<CR>")
    {
        return app::NewLine{};
    }
    if (key == "<BS>")
    {
        return app::DeleteText{core::DeleteDirection::backward,
                               nenenib::core::DeleteUnit::character};
    }
    if (key == "<Tab>")
    {
        return app::InsertText{"\t"};
    }
    return key.starts_with('<')
               ? std::nullopt
               : std::optional<app::EditorIntent>{app::InsertText{std::string(key)}};
}

[[nodiscard]] std::optional<app::EditorIntent> routed(const app::EditorController &controller,
                                                      core::EditMode mode, std::string_view key)
{
    if (controller.command_line_active())
    {
        return line_key(key);
    }
    if (controller.explorer_focused())
    {
        return explorer_key(key);
    }
    switch (mode)
    {
    case core::EditMode::ordinary:
        return ordinary_key(key);
    case core::EditMode::vim:
        break;
    }
    const auto parsed = vim_keys_of(key);
    return parsed.size() == 1U ? std::optional<app::EditorIntent>{app::VimKeyPress{parsed.front()}}
                               : std::nullopt;
}

// ---------------------------------------------------------------- 走り

[[nodiscard]] bool signal_of(const Action &action)
{
    const auto *intent = std::get_if<app::EditorIntent>(&action);
    return intent != nullptr && (std::holds_alternative<app::WorkCompleted>(*intent) ||
                                 std::holds_alternative<app::AutosaveTick>(*intent) ||
                                 std::holds_alternative<app::PreviewTick>(*intent) ||
                                 std::holds_alternative<app::LanguageServerTick>(*intent) ||
                                 std::holds_alternative<app::MapTimeout>(*intent));
}

void open_files(Editing &editing, const Bodies &bodies)
{
    for (std::size_t index = 0; index < paths.size(); ++index)
    {
        editing.files().hold_at(std::string(paths.at(index)), bodies.at(index));
    }
    static_cast<void>(editing.controller().apply(app::VisibleLines{
        visible_lines, core::TextViewGeometry{400, 20, 96, 10.0F}, 800, 600, 20}));
    static_cast<void>(editing.controller().apply(app::OpenDocument{path_at(0U)}));
}

// 1 歩を当てて不変条件 1〜3 を見る。mode は直前の frame の編集のモード。
[[nodiscard]] Reason stepped(Editing &editing, const Action &action, app::EditorFrame &frame)
{
    auto &controller = editing.controller();
    if (const auto *intent = std::get_if<app::EditorIntent>(&action))
    {
        static_cast<void>(controller.apply(*intent));
    }
    else
    {
        const auto chosen = routed(controller, frame.mode, std::get<Typed>(action));
        if (chosen.has_value())
        {
            static_cast<void>(controller.apply(chosen.value()));
        }
    }
    return frame_violation(controller, frame);
}

[[nodiscard]] std::string body_of(const Bodies &bodies, const app::DocumentView &view)
{
    if (!view.path.has_value())
    {
        return {};
    }
    const auto found = std::ranges::find(paths, view.path.value().text());
    return found == paths.end()
               ? std::string("(unknown path)")
               : bodies.at(static_cast<std::size_t>(std::distance(paths.begin(), found)));
}

void settle_for_undo(app::EditorController &controller)
{
    if (controller.is_composing())
    {
        static_cast<void>(controller.apply(app::CancelComposition{}));
    }
    if (controller.command_line_active())
    {
        static_cast<void>(controller.apply(app::CancelCommand{}));
    }
    if (controller.explorer_focused())
    {
        static_cast<void>(controller.apply(app::ExplorerKey{app::ExplorerKeyKind::esc}));
    }
    static_cast<void>(controller.apply(app::SettleRecentTab{}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::ordinary}));
}

// 不変条件 5: 帯の各タブで undo を歩の数 + 4 回打つと開いたときの本文に戻る。
[[nodiscard]] Reason undo_violation(Editing &editing, const Case &input)
{
    auto &controller = editing.controller();
    settle_for_undo(controller);
    const std::size_t presses = input.second.size() + 4U;
    const std::size_t count = controller.documents().size();
    for (std::size_t position = 0; position < count; ++position)
    {
        static_cast<void>(controller.apply(app::SwitchTab{position}));
        for (std::size_t press = 0; press < presses; ++press)
        {
            static_cast<void>(controller.apply(app::HistoryAction{core::HistoryDirection::undo}));
        }
        const auto views = controller.documents();
        if (controller.text().text() != body_of(input.first, views.at(position)))
        {
            return "5: undoing every edit of a tab does not give the text it was opened with";
        }
    }
    return {};
}

// 合図の歩はその場で観測を変えない（変えたら破れ）。使う人の歩の観測は列に積む。
[[nodiscard]] Reason observed(const Step &step, std::string now, std::string &last,
                              std::vector<std::string> &seen)
{
    if (signal_of(step.second))
    {
        return now == last ? Reason{}
                           : "4: a WorkCompleted or an AutosaveTick changes what the user sees";
    }
    last = now;
    seen.push_back(std::move(now));
    return {};
}

// 列を流し、使う人の各歩の後（合図の歩の後は除く）と最後に観測する。
[[nodiscard]] Outcome outcome_of(const Case &input, bool with_undo)
{
    Editing editing;
    open_files(editing, input.first);
    app::EditorFrame frame = editing.controller().frame();
    std::vector<std::string> seen;
    std::string last = observation(editing.controller(), frame);
    for (const auto &step : input.second)
    {
        Reason reason = stepped(editing, step.second, frame);
        reason = reason.empty()
                     ? observed(step, observation(editing.controller(), frame), last, seen)
                     : reason;
        if (!reason.empty())
        {
            return Outcome{reason, std::move(seen)};
        }
    }
    seen.push_back(observation(editing.controller(), frame));
    return Outcome{with_undo ? undo_violation(editing, input) : Reason{}, std::move(seen)};
}

// ---------------------------------------------------------------- 合図を挟む（不変条件 4）

[[nodiscard]] bool moves_caret(const Step &step)
{
    const auto *intent = std::get_if<app::EditorIntent>(&step.second);
    return intent != nullptr && std::holds_alternative<app::MoveCaret>(*intent);
}

// 0 個以上の合図を 1 つずつ確率 1/3 で足す。どの歩の前にも WorkCompleted を挟める（#1296 で
// 上下の移動の前の絞りを外した）。
void append_signals(Random &random, std::vector<Step> &steps)
{
    while (below(random, 3U) == 0U)
    {
        const bool work = below(random, 2U) == 0U;
        steps.emplace_back(work ? "work_completed" : "autosave_tick",
                           work ? Action{app::EditorIntent{app::WorkCompleted{}}}
                                : Action{app::EditorIntent{app::AutosaveTick{}}});
    }
}

// 歩の間（先頭と末尾を含む）に、seed から決まる位置へ WorkCompleted か AutosaveTick を挟む。
[[nodiscard]] Case signalled(const Case &input, std::uint64_t seed)
{
    Random random{seed};
    Case output{input.first, {}};
    const auto &steps = input.second;
    append_signals(random, output.second);
    for (const Step &step : steps)
    {
        output.second.push_back(step);
        append_signals(random, output.second);
    }
    return output;
}

[[nodiscard]] std::uint64_t seed_of(const Case &input)
{
    return 0x1288U + input.second.size() * 7919U + input.first.at(0).size();
}

[[nodiscard]] Reason case_violation(const Case &input)
{
    const Outcome plain = outcome_of(input, true);
    if (!plain.first.empty())
    {
        return plain.first;
    }
    const Outcome mixed = outcome_of(signalled(input, seed_of(input)), false);
    if (!mixed.first.empty())
    {
        return mixed.first;
    }
    return plain.second == mixed.second
               ? Reason{}
               : "4: a WorkCompleted or an AutosaveTick changes what the user sees";
}

// ---------------------------------------------------------------- 生成器

[[nodiscard]] Step intent_step(std::string label, app::EditorIntent intent)
{
    return Step{std::move(label), Action{std::move(intent)}};
}

[[nodiscard]] Step typed_step(std::string_view key)
{
    return Step{"key " + std::string(key), Action{Typed{key}}};
}

template <std::size_t N>
[[nodiscard]] std::string_view pick(Random &random, const std::array<std::string_view, N> &pool)
{
    return pool.at(below(random, N));
}

using Steps = std::vector<Step>;

void make_keys(Random &random, Steps &steps)
{
    const std::size_t count = 1U + below(random, 4U);
    for (std::size_t index = 0; index < count; ++index)
    {
        steps.push_back(typed_step(pick(random, keys)));
    }
}

void make_ex(Random &random, Steps &steps)
{
    steps.push_back(typed_step(":"));
    const std::string_view command = pick(random, ex_commands);
    steps.push_back(
        intent_step("ex " + std::string(command), app::CommandText{std::string(command)}));
    steps.push_back(typed_step(below(random, 4U) == 0U ? "<Esc>" : "<CR>"));
}

void make_palette(Random &random, Steps &steps)
{
    steps.push_back(intent_step("palette", app::OpenCommandPalette{}));
    for (std::size_t index = below(random, 3U); index > 0U; --index)
    {
        const std::string_view text = pick(random, palette_texts);
        steps.push_back(
            intent_step("palette " + std::string(text), app::CommandText{std::string(text)}));
    }
    const std::size_t ending = below(random, 4U);
    if (ending == 0U)
    {
        steps.push_back(intent_step("palette cancel", app::CancelCommand{}));
    }
    else if (ending == 1U)
    {
        steps.push_back(intent_step("palette scroll", app::ScrollPaletteList{1}));
    }
    else
    {
        const std::size_t choice = below(random, 3U);
        steps.push_back(intent_step("palette choose " + std::to_string(choice),
                                    app::ActivateCommandChoice{choice}));
    }
}

void make_find(Random &random, Steps &steps)
{
    const std::size_t kind = below(random, 4U);
    if (kind == 3U)
    {
        steps.push_back(
            intent_step("search_hop", app::SearchHop{core::VimSearchDirection::forward}));
        return;
    }
    steps.push_back(kind == 0U   ? intent_step("find", app::BeginFind{})
                    : kind == 1U ? intent_step("replace", app::BeginReplace{})
                                 : intent_step("go_to_line", app::BeginGoToLine{}));
    const std::string_view text = pick(random, find_texts);
    steps.push_back(intent_step("line " + std::string(text), app::CommandText{std::string(text)}));
    const bool all = below(random, 2U) == 0U;
    steps.push_back(intent_step(all ? "submit all" : "submit", app::SubmitCommand{all}));
    if (below(random, 2U) == 0U)
    {
        steps.push_back(intent_step("line Q", app::CommandText{"Q"}));
        steps.push_back(intent_step("submit", app::SubmitCommand{below(random, 2U) == 0U}));
    }
}

void make_tabs(Random &random, Steps &steps)
{
    const std::size_t kind = below(random, 7U);
    const std::size_t position = below(random, tab_reach);
    const std::array<Step, 7> made{
        intent_step("new_tab", app::NewTab{}),
        intent_step("switch " + std::to_string(position), app::SwitchTab{position}),
        intent_step("walk next", app::WalkRecentTab{core::TabStep::next}),
        intent_step("walk previous", app::WalkRecentTab{core::TabStep::previous}),
        intent_step("close " + std::to_string(position), app::CloseTab{position}),
        intent_step("open " + std::to_string(position % 3U),
                    app::OpenDocument{path_at(position % 3U)}),
        intent_step("settle", app::SettleRecentTab{})};
    steps.push_back(made.at(kind));
}

// 上下の移動の続き（#1296）: 通常モードの上下と行頭・行末の移動を 2〜4 個続ける。狙いの横位置
// （wanted_x）は続けた上下の移動の間だけ生きるので、合図がその間に挟まる形を作る。
void make_moves(Random &random, Steps &steps)
{
    for (std::size_t count = 2U + below(random, 3U); count > 0U; --count)
    {
        const std::size_t motion = below(random, 4U);
        steps.push_back(intent_step(
            "move " + std::to_string(motion + 2U),
            app::MoveCaret{motions.at(motion + 2U), core::SelectionAnchoring::collapse}));
    }
}

void make_edit(Random &random, Steps &steps)
{
    const std::size_t kind = below(random, 8U);
    const std::size_t motion = below(random, motions.size());
    const bool extend = below(random, 2U) == 0U;
    const std::string_view text = pick(random, typed_texts);
    const std::array<Step, 8> made{
        intent_step("insert " + std::string(text), app::InsertText{std::string(text)}),
        intent_step("move " + std::to_string(motion) + (extend ? "+" : ""),
                    app::MoveCaret{motions.at(motion), extend
                                                           ? core::SelectionAnchoring::extend
                                                           : core::SelectionAnchoring::collapse}),
        intent_step("delete", app::DeleteText{extend ? core::DeleteDirection::backward
                                                     : core::DeleteDirection::forward,
                                              core::DeleteUnit::character}),
        intent_step("select_all", app::SelectAll{}),
        intent_step(extend ? "copy" : "cut",
                    app::ClipboardAction{extend ? app::ClipboardOperation::copy
                                                : app::ClipboardOperation::cut}),
        intent_step("paste", app::ClipboardAction{app::ClipboardOperation::paste}),
        intent_step(extend ? "undo" : "redo",
                    app::HistoryAction{extend ? core::HistoryDirection::undo
                                              : core::HistoryDirection::redo}),
        intent_step("date_time", app::InsertDateTime{})};
    steps.push_back(made.at(kind));
}

void make_mode(Random &random, Steps &steps)
{
    const bool vim = below(random, 3U) != 0U;
    steps.push_back(
        intent_step(vim ? "mode vim" : "mode ordinary",
                    app::SelectEditMode{vim ? core::EditMode::vim : core::EditMode::ordinary}));
}

void make_panels(Random &random, Steps &steps)
{
    const std::size_t kind = below(random, 7U);
    const std::size_t key = below(random, explorer_keys.size());
    const std::array<Step, 7> made{
        intent_step("explorer", app::ToggleExplorer{}),
        intent_step("explorer key " + std::to_string(key), app::ExplorerKey{explorer_keys.at(key)}),
        intent_step("preview", app::TogglePreview{}),
        intent_step("preview_tick", app::PreviewTick{}),
        intent_step("preview_task", app::TogglePreviewTask{0U}),
        intent_step("print", app::PrintDocument{app::PrintSettings{
                                 "Scripted Printer", app::PrintPaper{794, 1123}, {}, {}}}),
        intent_step("print refused",
                    app::PrintDocument{std::unexpected(app::PrintFailure::no_printer)})};
    steps.push_back(made.at(kind));
}

[[nodiscard]] core::Composition composition_of(Random &random)
{
    std::string text;
    for (std::size_t index = below(random, 3U) + 1U; index > 0U; --index)
    {
        text += pick(random, kana);
    }
    const std::size_t size = text.size();
    return composed_of(
        std::move(text),
        {core::CompositionClause{core::OffsetRange{core::Offset{0}, core::Offset{size}},
                                 core::ClauseEmphasis::target}},
        size);
}

void make_ime(Random &random, Steps &steps)
{
    for (std::size_t index = below(random, 2U) + 1U; index > 0U; --index)
    {
        core::Composition composition = composition_of(random);
        std::string label = "compose " + composition.utf8;
        steps.push_back(intent_step(std::move(label), app::ComposeText{std::move(composition)}));
    }
    const bool commit = below(random, 3U) != 0U;
    steps.push_back(commit ? intent_step("commit 変換", app::CommitText{"変換"})
                           : intent_step("cancel composition", app::CancelComposition{}));
}

using Maker = void (*)(Random &, Steps &);
// 行の重みは並べた回数。鍵と編集を厚くし、面・行・タブ・パネル・IME を同じくらいに混ぜる。
// 上下の移動の続き（make_moves）は #1296 で足した。
constexpr std::array<Maker, 21> makers{
    make_keys, make_keys, make_keys,   make_keys,   make_keys,    make_edit, make_edit,
    make_edit, make_mode, make_mode,   make_ex,     make_palette, make_find, make_tabs,
    make_tabs, make_tabs, make_panels, make_panels, make_ime,     make_ime,  make_moves};

[[nodiscard]] std::string random_body(Random &random)
{
    const std::size_t lines = below(random, 6U);
    const std::string_view newline = below(random, 2U) == 0U ? "\n" : "\r\n";
    std::string body;
    for (std::size_t line = 0; line < lines; ++line)
    {
        body += line == 0U ? std::string_view{} : newline;
        for (std::size_t token = below(random, 4U); token > 0U; --token)
        {
            body += pick(random, body_tokens);
        }
    }
    return body;
}

[[nodiscard]] Case random_case(Random &random)
{
    Case input;
    for (auto &body : input.first)
    {
        body = random_body(random);
    }
    const std::size_t target = step_floor + below(random, step_spread);
    while (input.second.size() < target)
    {
        makers.at(below(random, makers.size()))(random, input.second);
    }
    return input;
}

// 破れた組を縮める: 歩を 1 つずつ外し、破れが残るなら外したままにする。
void minimize(Case &input, Check check)
{
    for (std::size_t index = input.second.size(); index-- > 0;)
    {
        Case smaller = input;
        smaller.second.erase(std::next(smaller.second.begin(), static_cast<std::ptrdiff_t>(index)));
        if (!check(smaller).empty())
        {
            input = std::move(smaller);
        }
    }
}

[[nodiscard]] std::string escaped(std::string_view text)
{
    std::string shown;
    for (const char unit : text)
    {
        shown += unit == '\r'   ? std::string("\\r")
                 : unit == '\n' ? std::string("\\n")
                 : unit == '\t' ? std::string("\\t")
                                : std::string(1, unit);
    }
    return shown;
}

void report(const char *family, Case input, Check check)
{
    minimize(input, check);
    const Reason reason = check(input);
    std::fprintf(stderr, "  %s: %.*s\n", family, static_cast<int>(reason.size()), reason.data());
    for (std::size_t index = 0; index < input.first.size(); ++index)
    {
        std::fprintf(stderr, "    body %zu: \"%s\"\n", index,
                     escaped(input.first.at(index)).c_str());
    }
    std::fprintf(stderr, "    steps:");
    for (const auto &step : input.second)
    {
        std::fprintf(stderr, " [%s]", escaped(step.first).c_str());
    }
    std::fprintf(stderr, "\n    signalled:");
    for (const auto &step : signalled(input, seed_of(input)).second)
    {
        std::fprintf(stderr, " [%s]", escaped(step.first).c_str());
    }
    std::fprintf(stderr, "\n");
}

[[nodiscard]] std::size_t broken_cases(const char *family, std::uint64_t seed, Check check)
{
    Random random{seed};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < family_cases; ++index)
    {
        const Case input = random_case(random);
        if (!check(input).empty() && broken++ == 0U)
        {
            report(family, input, check);
        }
    }
    return broken;
}

// ---------------------------------------------------------------- 決定性・届く所・検査そのもの

[[nodiscard]] std::vector<std::string> labels_of(const Case &input)
{
    std::vector<std::string> labels(input.first.begin(), input.first.end());
    for (const auto &step : input.second)
    {
        labels.push_back(step.first);
    }
    return labels;
}

void verify_determinism()
{
    Random left{12880U};
    Random right{12880U};
    bool same_cases = true;
    bool same_outcomes = true;
    for (std::size_t round = 0; round < 2U; ++round)
    {
        const Case first = random_case(left);
        same_cases = same_cases && labels_of(first) == labels_of(random_case(right));
        same_outcomes = same_outcomes && outcome_of(first, false) == outcome_of(first, false);
    }
    expect(same_cases, "the mixed generator gives the same cases for the same seed");
    expect(same_outcomes, "the same mixed case gives the same observations twice");
}

using Reached = std::array<std::size_t, 8>;

// 今の状態が届いた印（面・入力行・INSERT・焦点・変換・2 本以上・歩き・未保存）を数える。
void count_reached(Reached &reached, const app::EditorController &controller,
                   const app::EditorFrame &frame)
{
    const std::array<bool, 8> now{
        controller.command_palette_active(),
        controller.command_line_active() && !controller.command_palette_active(),
        controller.vim_state().mode == core::VimMode::insert,
        controller.explorer_focused(),
        controller.is_composing(),
        frame.tabs.size() > 1U,
        controller.tab_walking(),
        frame.tabs.at(frame.active_tab).save_state == core::SaveState::modified};
    for (std::size_t flag = 0; flag < now.size(); ++flag)
    {
        reached.at(flag) += now.at(flag) ? 1U : 0U;
    }
}

// seed から作った cases 件の列を流し、届いた状態を数える。
[[nodiscard]] Reached reached_by(std::uint64_t seed, std::size_t cases)
{
    Random random{seed};
    Reached reached{};
    for (std::size_t index = 0; index < cases; ++index)
    {
        const Case input = random_case(random);
        Editing editing;
        open_files(editing, input.first);
        app::EditorFrame frame = editing.controller().frame();
        for (const auto &step : input.second)
        {
            static_cast<void>(stepped(editing, step.second, frame));
            count_reached(reached, editing.controller(), frame);
        }
    }
    return reached;
}

// 生成した列が面・入力行・Vim の INSERT・エクスプローラーの焦点・変換・複数タブ・歩きの途中・
// 本文の変更に届く。どれかが 0 なら性質の試験はその状態を見ていない。seed は #1296 で生成器に
// 上下の移動の続きを足したとき 12881 から 12883 にした（12881 の 4 件は INSERT に届かなくなった・
// 12881〜12896 の 16 個のうち 13 個が全部に届く）。
void verify_generator_reaches_the_states()
{
    const Reached reached = reached_by(12883U, 4U);
    expect(std::ranges::all_of(reached, [](std::size_t count) { return count > 0U; }),
           "the mixed cases reach the palette, an input line, INSERT, the explorer focus, a "
           "composition, several tabs, a walk and a modified document");
}

[[nodiscard]] Step moved(core::CaretMotion motion)
{
    return intent_step("move", app::MoveCaret{motion, core::SelectionAnchoring::collapse});
}

// 2 つの移動の間（列の 5 歩目の前）に合図の列 signals を挟んだ列。
[[nodiscard]] Case between_moves(const Case &input, const std::vector<Step> &signals)
{
    Case output = input;
    output.second.insert(std::next(output.second.begin(), 4), signals.begin(), signals.end());
    return output;
}

// 通常モードの上下の移動の狙いの横位置（wanted_x）を、どの合図を挟んでも保つ（#1296 の最小の
// 再現・直す前は WorkCompleted を挟むと 3 行 2 桁へ着いた）。合図が 2 つ続いても同じ。
void verify_known_cases()
{
    const Case wanted{Bodies{"abcdef\nx\nabcdef", "", ""},
                      {moved(core::CaretMotion::line_end), moved(core::CaretMotion::document_start),
                       moved(core::CaretMotion::line_end), moved(core::CaretMotion::next_line),
                       moved(core::CaretMotion::next_line)}};
    const auto plain = outcome_of(wanted, false);
    expect(plain.first.empty() && plain.second.back().starts_with("abcdef\nx\nabcdef|3,7,"),
           "two downward moves through a short line land back on the wanted column");
    const std::array<std::vector<Step>, 5> signal_runs{
        std::vector<Step>{intent_step("tick", app::AutosaveTick{})},
        std::vector<Step>{intent_step("work", app::WorkCompleted{})},
        std::vector<Step>{intent_step("work", app::WorkCompleted{}),
                          intent_step("preview", app::PreviewTick{})},
        std::vector<Step>{intent_step("server", app::LanguageServerTick{}),
                          intent_step("map", app::MapTimeout{})},
        std::vector<Step>{intent_step("work", app::WorkCompleted{}),
                          intent_step("work", app::WorkCompleted{})}};
    for (const auto &signals : signal_runs)
    {
        const auto mixed = outcome_of(between_moves(wanted, signals), false);
        expect(mixed.first.empty() && mixed.second == plain.second,
               "signals between two vertical moves keep the wanted column (#1296)");
    }
}

// 合図の種類ごとの出た回数（WorkCompleted・AutosaveTick・MoveCaret・MoveCaret の直前の
// WorkCompleted）。
using SignalCounts = std::array<std::size_t, 4>;

void count_signals(SignalCounts &counts, const std::vector<Step> &steps)
{
    for (std::size_t index = 0; index < steps.size(); ++index)
    {
        const Step &step = steps.at(index);
        const bool work = step.first == "work_completed";
        counts.at(0) += work ? 1U : 0U;
        counts.at(1) += step.first == "autosave_tick" ? 1U : 0U;
        counts.at(2) += moves_caret(step) ? 1U : 0U;
        counts.at(3) +=
            work && index + 1U < steps.size() && moves_caret(steps.at(index + 1U)) ? 1U : 0U;
    }
}

// 本番の組（性質の seed の family_cases 件）の合図を挟んだ列に、WorkCompleted・AutosaveTick・
// MoveCaret と、MoveCaret の直前の WorkCompleted が出る。0 なら性質 4 はその形を見ていない。
void verify_signals_reach_moves()
{
    Random random{12882U};
    SignalCounts counts{};
    for (std::size_t index = 0; index < family_cases; ++index)
    {
        const Case input = random_case(random);
        count_signals(counts, signalled(input, seed_of(input)).second);
    }
    expect(counts.at(0) >= signal_floor && counts.at(1) >= signal_floor &&
               counts.at(2) >= signal_floor && counts.at(3) > 0U,
           "the signalled cases hold WorkCompleted, AutosaveTick and MoveCaret steps and a "
           "WorkCompleted right before a MoveCaret");
}

// 検査そのものが破れを見つける（観測の違い・行の外のキャレット・1 文字の途中）。
void verify_checks_catch_a_break()
{
    const std::string text = "e\u0301x";
    const auto lines = lines_of(text, false);
    Editing editing;
    app::EditorFrame frame = editing.controller().frame();
    frame.caret.position = core::TextPosition{core::LineNumber{1}, core::Column{2}};
    const bool inside = !caret_violation(text, lines, frame).empty();
    frame.caret.position = core::TextPosition{core::LineNumber{2}, core::Column{1}};
    const bool outside = !caret_violation(text, lines, frame).empty();
    frame.caret.position = core::TextPosition{core::LineNumber{1}, core::Column{3}};
    const bool kept = caret_violation(text, lines, frame).empty();
    expect(inside && outside && kept,
           "the caret check catches a caret inside a character and on a missing line");
    const std::string crlf = "a\r\nb";
    const auto paired = lines_of(crlf, true);
    frame.caret.position = core::TextPosition{core::LineNumber{1}, core::Column{3}};
    expect(!caret_violation(crlf, paired, frame).empty(),
           "the caret check catches a caret inside a CRLF pair");
}

} // namespace

void verify_controller_mixed_property_scope()
{
    verify_determinism();
    verify_generator_reaches_the_states();
    verify_checks_catch_a_break();
    verify_known_cases();
    verify_signals_reach_moves();
    expect(broken_cases("mixed controller intents keep their invariants", 12882U, case_violation) ==
               0U,
           "mixed edits, Vim keys, tabs, the palette, Ex and find lines, the explorer, the "
           "preview, printing and IME keep the frame, the band, the caret, closed modes, signal "
           "neutrality and the undo round trip");
}
} // namespace nenenib::tests
