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
// line / column の比較が意味を持たないので `text` の 1 行だけを書く。鍵の記法の表
// （src/core/VimKeyNames.hpp）に無い `<…>` を含む件と、欄の数が違う行は `skip <name> <理由>`。
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "OpenDocument.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VimKeyName.hpp"
#include "VimKeyNames.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <fcntl.h>
#include <io.h>

#include <cstddef>
#include <cstdio>
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
using nenenib::core::VimKeyName;
using nenenib::tests::Bytes;
using nenenib::tests::Editing;
using Fields = std::vector<std::string>;

// 1 件の欄の数（name text keys expected line column register register_kind）。
constexpr std::size_t field_count = 8;
constexpr std::size_t read_chunk = 65536;

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

// verify_vim_fixture と同じ道で 1 件を流し、違った欄ごとに 1 行を書く。
void replay_one(const Fields &fields, std::size_t line_number, std::size_t column)
{
    const std::string &name = fields.at(0);
    const std::string text = unescaped(fields.at(1));
    const std::string keys = unescaped(fields.at(2));
    const std::string expected = unescaped(fields.at(3));
    const std::string register_text = unescaped(fields.at(6));
    const std::string register_kind = unescaped(fields.at(7));

    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{text});
    static_cast<void>(controller.apply_frame(VisibleLines{nenenib::tests::vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{nenenib::tests::sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    nenenib::tests::vim_normal(controller, keys);
    const TextPosition caret = controller.frame().caret.position;
    const std::string actual_register = controller.vim_state().unnamed_register.value().text;
    const std::string actual_kind =
        nenenib::tests::vim_register_kind(controller.vim_state().unnamed_register.value());
    const std::string body = nenenib::tests::whole_vim_body(controller);
    if (body != expected)
    {
        write_mismatch(name, "text", expected, body);
        return;
    }
    std::size_t differences = 0;
    if (caret.line.value != line_number)
    {
        write_mismatch(name, "line", std::to_string(line_number), std::to_string(caret.line.value));
        ++differences;
    }
    const std::size_t actual_column = nenenib::tests::vim_byte_column(expected, caret);
    if (actual_column != column)
    {
        write_mismatch(name, "column", std::to_string(column), std::to_string(actual_column));
        ++differences;
    }
    if (actual_register != register_text)
    {
        write_mismatch(name, "register", register_text, actual_register);
        ++differences;
    }
    if (actual_kind != register_kind)
    {
        write_mismatch(name, "register_kind", register_kind, actual_kind);
        ++differences;
    }
    if (differences == 0)
    {
        write_line("ok " + name);
    }
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
    const Fields fields = fields_of(line);
    const std::string name = fields.at(0);
    if (fields.size() != field_count)
    {
        write_line("skip " + name + " expected " + std::to_string(field_count) + " fields, got " +
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
        write_line("skip " + name + " line and column must be decimal numbers");
        return;
    }
    replay_one(fields, line_number.value(), column.value());
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
