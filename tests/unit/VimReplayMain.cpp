// Vim の差分 fuzz の再生器 nib_vim_replay（Issue #954）。eng/vim-fuzz.py が本物の Vim 9.1 で測った
// 件を 1 行 1 件のタブ区切りで受け、fixture の再生（verify_vim_fixture）と同じ道で Nib に流して、
// 本文・line・column・無名レジスタの本文と種類を比べる。CTest には載せない（設計席の道具）。
//
// 入力: 標準入力か `--fixtures <path>` の 1 つ。1 行が
//   name \t text \t keys \t expected \t line \t column \t register \t register_kind
// で、値の中の改行・タブ・バックスラッシュ・CR は `\n` `\t` `\\` `\r` と書く。形式はこの道具だけの
// もので、製品の core は触れない。
// 出力（UTF-8・改行は `\n`）: 1 件ごとに `ok <name>`、違えば違った欄ごとに 1 行の
// `mismatch <name> <欄> expected=<値> actual=<値>`（値は入力と同じエスケープ）。本文が違う件は
// line / column の比較が意味を持たないので `text` の 1 行だけを書く。
// viewport つきの件（生成器 5・#1224）は 8 欄の後ろに
//   visible_lines \t first_visible \t start_line \t start_column \t expected_first_visible \t
//   expected_scroll_lines \t scrolloff
// の 7 欄を足す。窓は fixture の再生と同じ arrange_vim_viewport で置き、余白は窓を置いた後で
// `:set so=` を打つ（契約 --vim-view の placed_window と同じ順）。比べる欄に `first_visible` と
// `scroll`（Vim の 'scroll'）が加わる。鍵の記法の表
// （src/core/VimKeyNames.hpp）に無い `<…>` を含む件と、欄の数が違う行と、数の欄が
// std::uint32_t の範囲を超える行は `skip <name> <理由>`（#1233）。
// どちらの形も、最後の欄が `change=<m><c><u>`（各 0 か 1・oracle の fixture の `change` と同じ
// &modified・changedtick が進んだか・seq_cur が進んだか）なら、Vim の変更の印も比べる
// （ADR 0181・#1440）。比べる欄は `modified`（未保存の印）・`changed`（`'.` が置かれたか・Vim の
// changedtick は `u` でも進むので本文の位置ではなく `'.` で比べる）・`undo`（履歴の位置が 0 より
// 先か）。
#include "../vim/VimFixture.hpp"
#include "../vim/VimViewportFixture.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "OpenDocument.hpp"
#include "SaveState.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VimCount.hpp"
#include "VimKeyName.hpp"
#include "VimKeyNames.hpp"
#include "VimRegister.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nenenib::application::EditorController;
using nenenib::application::OpenDocument;
using nenenib::application::SelectEditMode;
using nenenib::application::VisibleLines;
using nenenib::core::EditMode;
using nenenib::core::TextPosition;
using nenenib::core::vim_key_names;
using nenenib::core::VimCount;
using nenenib::core::VimKeyName;
using nenenib::core::VimRegister;
using nenenib::tests::Bytes;
using nenenib::tests::Editing;
using nenenib::tests::VimFixture;
using nenenib::tests::VimViewportFixture;
using Fields = std::vector<std::string>;

// 1 件の欄の数（name text keys expected line column register register_kind）。
constexpr std::size_t field_count = 8;
// viewport つきの件の欄の数（上の 8 欄と viewport の 7 欄・#1224）。
constexpr std::size_t viewport_field_count = 15;
constexpr std::size_t viewport_numbers = viewport_field_count - field_count;
using ViewportValues = std::array<std::size_t, viewport_numbers>;
// 変更の印の欄（modified changed undo・ADR 0181）。
using ChangeMarks = std::array<bool, 3>;
// 本文の後の line と column。
using Place = std::pair<std::size_t, std::size_t>;
constexpr std::string_view change_prefix = "change=";
constexpr std::size_t read_chunk = 65536;
// 数の欄の上限。fixture の line・column と窓の値は std::uint32_t に狭めるので、超える欄は読めない
// 欄として skip にする（#1233・R2-1）。
constexpr std::size_t number_limit = std::numeric_limits<std::uint32_t>::max();

// `\` の後の 1 文字が表す文字（`\n` `\t` `\r` のほかは文字そのもの・`\\` は `\`）。
[[nodiscard]] char unescaped_character(char escaped) noexcept
{
    if (escaped == 'n')
    {
        return '\n';
    }
    if (escaped == 't')
    {
        return '\t';
    }
    return escaped == 'r' ? '\r' : escaped;
}

[[nodiscard]] std::string unescaped(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const bool escape = value.at(index) == '\\' && index + 1 < value.size();
        if (escape)
        {
            ++index;
        }
        result.push_back(escape ? unescaped_character(value.at(index)) : value.at(index));
    }
    return result;
}

[[nodiscard]] std::string escaped(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char character : value)
    {
        if (character == '\\')
        {
            result += "\\\\";
        }
        else if (character == '\n')
        {
            result += "\\n";
        }
        else if (character == '\t')
        {
            result += "\\t";
        }
        else if (character == '\r')
        {
            result += "\\r";
        }
        else
        {
            result.push_back(character);
        }
    }
    return result;
}

[[nodiscard]] Fields fields_of(std::string_view line)
{
    Fields result;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string_view::npos)
        {
            result.emplace_back(line.substr(start));
            return result;
        }
        result.emplace_back(line.substr(start, tab - start));
        start = tab + 1;
    }
}

[[nodiscard]] bool names_a_key(std::string_view keys, std::size_t index)
{
    for (const VimKeyName name : vim_key_names)
    {
        if (keys.substr(index).starts_with(name.text))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool letter(char character) noexcept
{
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}

[[nodiscard]] bool key_name_character(char character) noexcept
{
    return letter(character) || (character >= '0' && character <= '9') || character == '-';
}

// index から始まる `<名前>` の形（`<` の直後が英字で、英数字と `-` が続いて `>` で閉じる）。
// `<<` `i<` `a<` のような 1 文字の `<` は名前ではない。
[[nodiscard]] std::optional<std::string> key_name_at(std::string_view keys, std::size_t index)
{
    if (keys.at(index) != '<' || index + 1 == keys.size() || !letter(keys.at(index + 1)))
    {
        return std::nullopt;
    }
    std::size_t stop = index + 1;
    while (stop < keys.size() && key_name_character(keys.at(stop)))
    {
        ++stop;
    }
    if (stop == keys.size() || keys.at(stop) != '>')
    {
        return std::nullopt;
    }
    return std::string(keys.substr(index, stop - index + 1));
}

// 記法の表（src/core/VimKeyNames.hpp）に無い `<名前>`。
[[nodiscard]] std::optional<std::string> unknown_key_name(std::string_view keys)
{
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const auto name = key_name_at(keys, index);
        if (name.has_value() && !names_a_key(keys, index))
        {
            return name;
        }
    }
    return std::nullopt;
}

// 10 進の数の欄。空・数字でない文字・number_limit を超える値は無し。
[[nodiscard]] std::optional<std::size_t> number_of(std::string_view value)
{
    if (value.empty())
    {
        return std::nullopt;
    }
    std::size_t result = 0;
    for (const char character : value)
    {
        if (character < '0' || character > '9')
        {
            return std::nullopt;
        }
        result = result * 10 + static_cast<std::size_t>(character - '0');
        if (result > number_limit)
        {
            return std::nullopt;
        }
    }
    return result;
}

void write_line(const std::string &line)
{
    static_cast<void>(std::fwrite(line.data(), 1, line.size(), stdout));
    static_cast<void>(std::fputc('\n', stdout));
}

void write_mismatch(const std::string &name, std::string_view field, std::string_view expected,
                    std::string_view actual)
{
    write_line("mismatch " + name + " " + std::string(field) + " expected=" + escaped(expected) +
               " actual=" + escaped(actual));
}

// viewport の 7 欄（visible_lines first_visible start_line start_column expected_first_visible
// expected_scroll_lines scrolloff）から fixture の窓の値を作る。余白は最後の欄で別に読む。
// どの欄も number_of が number_limit 以下に収めてあるので、狭めで値は変わらない。
[[nodiscard]] VimViewportFixture viewport_of(const ViewportValues &values)
{
    return VimViewportFixture{
        static_cast<std::uint32_t>(values.at(0)), static_cast<std::uint32_t>(values.at(1)),
        static_cast<std::uint32_t>(values.at(2)), static_cast<std::uint32_t>(values.at(3)),
        static_cast<std::uint32_t>(values.at(4)), static_cast<std::uint32_t>(values.at(5))};
}

// 窓の欄を比べる（verify_vim_fixture の viewport と scroll lines の比較と同じ値）。
[[nodiscard]] std::size_t viewport_differences(const std::string &name,
                                               const VimViewportFixture &viewport,
                                               std::size_t first_visible, std::size_t scroll_lines)
{
    std::size_t differences = 0;
    if (first_visible != viewport.expected_first_visible)
    {
        write_mismatch(name, "first_visible", std::to_string(viewport.expected_first_visible),
                       std::to_string(first_visible));
        ++differences;
    }
    if (scroll_lines != viewport.expected_scroll_lines)
    {
        write_mismatch(name, "scroll", std::to_string(viewport.expected_scroll_lines),
                       std::to_string(scroll_lines));
        ++differences;
    }
    return differences;
}

// 本文の後の 4 欄（line column register register_kind）を比べる。
[[nodiscard]] std::size_t caret_and_register_differences(const VimFixture &fixture,
                                                         const TextPosition &caret,
                                                         const VimRegister &actual)
{
    const std::string name(fixture.name);
    const std::string actual_kind = nenenib::tests::vim_register_kind(actual);
    std::size_t differences = 0;
    if (caret.line.value != fixture.line)
    {
        write_mismatch(name, "line", std::to_string(fixture.line),
                       std::to_string(caret.line.value));
        ++differences;
    }
    const std::size_t actual_column =
        nenenib::tests::vim_byte_column(std::string(fixture.expected_text), caret);
    if (actual_column != fixture.column)
    {
        write_mismatch(name, "column", std::to_string(fixture.column),
                       std::to_string(actual_column));
        ++differences;
    }
    if (actual.text != fixture.register_text)
    {
        write_mismatch(name, "register", fixture.register_text, actual.text);
        ++differences;
    }
    if (actual_kind != fixture.register_kind)
    {
        write_mismatch(name, "register_kind", fixture.register_kind, actual_kind);
        ++differences;
    }
    return differences;
}

// 変更の印を比べる（verify_vim_fixture の変更の印と同じ口・ADR 0181）。
[[nodiscard]] std::size_t change_differences(const std::string &name,
                                             const EditorController &controller,
                                             const std::optional<ChangeMarks> &change)
{
    if (!change.has_value())
    {
        return 0;
    }
    const ChangeMarks actual{
        controller.documents().at(0).save_state == nenenib::core::SaveState::modified,
        controller.vim_marks().last_change.has_value(), controller.history().position() > 0};
    constexpr std::array<std::string_view, 3> names{"modified", "changed", "undo"};
    std::size_t differences = 0;
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        if (actual.at(index) != change.value().at(index))
        {
            write_mismatch(name, names.at(index), change.value().at(index) ? "1" : "0",
                           actual.at(index) ? "1" : "0");
            ++differences;
        }
    }
    return differences;
}

// 最後の欄が変更の印なら外して読む。形が違えば欄はそのまま残り、欄の数の検査が skip にする。
[[nodiscard]] std::optional<ChangeMarks> taken_change(Fields &fields)
{
    if (fields.empty() || !fields.back().starts_with(change_prefix))
    {
        return std::nullopt;
    }
    const std::string_view digits = std::string_view(fields.back()).substr(change_prefix.size());
    if (digits.size() != 3 || digits.find_first_not_of("01") != std::string_view::npos)
    {
        return std::nullopt;
    }
    fields.pop_back();
    return ChangeMarks{digits.at(0) == '1', digits.at(1) == '1', digits.at(2) == '1'};
}

// 本文を開いて Vim にし、fixture の窓を置いてから余白を打つ（契約 --vim-view の placed_window
// の順）。
void prepare(Editing &editing, const VimFixture &fixture, std::size_t scrolloff)
{
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(fixture.text)});
    static_cast<void>(controller.apply_frame(VisibleLines{nenenib::tests::vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{nenenib::tests::sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    nenenib::tests::arrange_vim_viewport(controller, fixture);
    if (scrolloff > 0)
    {
        static_cast<void>(
            nenenib::tests::run_ex(controller, "set so=" + std::to_string(scrolloff)));
    }
}

// verify_vim_fixture と同じ道で 1 件を流し、違った欄ごとに 1 行を書く。viewport つきの件は
// 窓を置いてから余白を打つ。
void replay_one(const Fields &fields, Place place, const std::optional<ViewportValues> &window,
                const std::optional<ChangeMarks> &change)
{
    const auto [line_number, column] = place;
    const std::optional<VimViewportFixture> viewport =
        window.has_value() ? std::optional{viewport_of(window.value())} : std::nullopt;
    const std::size_t scrolloff = window.has_value() ? window.value().at(viewport_numbers - 1) : 0;
    const std::string &name = fields.at(0);
    const std::string text = unescaped(fields.at(1));
    const std::string keys = unescaped(fields.at(2));
    const std::string expected = unescaped(fields.at(3));
    const std::string register_text = unescaped(fields.at(6));
    const std::string register_kind = unescaped(fields.at(7));

    const VimFixture fixture{name,
                             text,
                             keys,
                             expected,
                             static_cast<std::uint32_t>(line_number),
                             static_cast<std::uint32_t>(column),
                             register_text,
                             register_kind,
                             viewport};
    Editing editing;
    EditorController &controller = editing.controller();
    prepare(editing, fixture, scrolloff);
    nenenib::tests::vim_normal(controller, keys);
    const TextPosition caret = controller.frame().caret.position;
    const std::size_t first_visible = controller.frame().first_visible.value;
    const std::size_t fallback =
        std::max<std::size_t>(viewport.has_value() ? viewport.value().visible_lines / 2 : 1, 1);
    const std::size_t scroll_lines =
        controller.vim_state().scroll_lines.value_or(VimCount{fallback}).value;
    const VimRegister actual_register = controller.vim_state().unnamed_register.value();
    const std::string body = nenenib::tests::whole_vim_body(controller);
    if (body != expected)
    {
        write_mismatch(name, "text", expected, body);
        return;
    }
    std::size_t differences = caret_and_register_differences(fixture, caret, actual_register);
    if (viewport.has_value())
    {
        differences += viewport_differences(name, viewport.value(), first_visible, scroll_lines);
    }
    differences += change_differences(name, controller, change);
    if (differences == 0)
    {
        write_line("ok " + name);
    }
}

// viewport の 7 欄を数として読む。読めない欄があれば無し。
[[nodiscard]] std::optional<ViewportValues> viewport_values(const Fields &fields)
{
    ViewportValues values{};
    for (std::size_t index = 0; index < viewport_numbers; ++index)
    {
        const auto value = number_of(fields.at(field_count + index));
        if (!value.has_value())
        {
            return std::nullopt;
        }
        values.at(index) = value.value();
    }
    return values;
}

[[nodiscard]] bool known_field_count(std::size_t count) noexcept
{
    return count == field_count || count == viewport_field_count;
}

void replay_line(std::string_view line)
{
    if (!line.empty() && line.back() == '\r')
    {
        line.remove_suffix(1);
    }
    if (line.empty())
    {
        return;
    }
    Fields fields = fields_of(line);
    const std::optional<ChangeMarks> change = taken_change(fields);
    const std::string name = fields.at(0);
    if (!known_field_count(fields.size()))
    {
        write_line("skip " + name + " expected " + std::to_string(field_count) + " or " +
                   std::to_string(viewport_field_count) + " fields, got " +
                   std::to_string(fields.size()));
        return;
    }
    const auto unknown = unknown_key_name(unescaped(fields.at(2)));
    if (unknown.has_value())
    {
        write_line("skip " + name + " unknown key name " + unknown.value());
        return;
    }
    const auto line_number = number_of(fields.at(4));
    const auto column = number_of(fields.at(5));
    if (!line_number.has_value() || !column.has_value())
    {
        write_line("skip " + name + " line and column must be decimal numbers up to " +
                   std::to_string(number_limit));
        return;
    }
    if (fields.size() == field_count)
    {
        replay_one(fields, Place{line_number.value(), column.value()}, std::nullopt, change);
        return;
    }
    const auto values = viewport_values(fields);
    if (!values.has_value())
    {
        write_line("skip " + name + " viewport fields must be decimal numbers up to " +
                   std::to_string(number_limit));
        return;
    }
    replay_one(fields, Place{line_number.value(), column.value()}, values, change);
}

[[nodiscard]] std::string whole_stream(std::FILE *stream)
{
    std::string contents;
    std::vector<char> chunk(read_chunk);
    while (true)
    {
        const std::size_t count = std::fread(chunk.data(), 1, chunk.size(), stream);
        contents.append(chunk.data(), count);
        if (count < chunk.size())
        {
            return contents;
        }
    }
}

[[nodiscard]] std::optional<std::string> input_of(int argc, char **argv)
{
    const std::vector<std::string_view> arguments(argv + 1, argv + argc);
    if (arguments.empty())
    {
        return whole_stream(stdin);
    }
    if (arguments.size() != 2 || arguments.at(0) != "--fixtures")
    {
        return std::nullopt;
    }
    std::FILE *file = nullptr;
    if (fopen_s(&file, std::string(arguments.at(1)).c_str(), "rb") != 0 || file == nullptr)
    {
        return std::nullopt;
    }
    std::string contents = whole_stream(file);
    static_cast<void>(std::fclose(file));
    return contents;
}
} // namespace

int main(int argc, char **argv)
{
    // 改行を CRLF に化けさせない（出力は `\n` だけ・入力の `\r` もそのまま読む）。
    static_cast<void>(_setmode(_fileno(stdin), _O_BINARY));
    static_cast<void>(_setmode(_fileno(stdout), _O_BINARY));
    const auto input = input_of(argc, argv);
    if (!input.has_value())
    {
        static_cast<void>(
            std::fputs("usage: nib_vim_replay [--fixtures <path>]  (one fixture per line, "
                       "tab-separated; reads stdin when no path is given)\n",
                       stderr));
        return 2;
    }
    const std::string_view all = input.value();
    std::size_t start = 0;
    while (start < all.size())
    {
        const std::size_t stop = all.find('\n', start);
        const std::size_t end = stop == std::string_view::npos ? all.size() : stop;
        replay_line(all.substr(start, end - start));
        start = end + 1;
    }
    static_cast<void>(std::fflush(stdout));
    return 0;
}
