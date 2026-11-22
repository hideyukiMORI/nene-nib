// よく知られた言語サーバーの表と、命令の行の書き方（LSP 段 S4・#1945）。
#include "KnownLanguageServers.hpp"

#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "KnownLanguageServer.hpp"
#include "Language.hpp"
#include "LanguageServerCandidate.hpp"
#include "LspJsonRead.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
// 言語の束（行が書く言語）。
constexpr std::array c_family{Language::c, Language::cpp};
constexpr std::array python_only{Language::python};
constexpr std::array script_family{Language::typescript, Language::javascript};
constexpr std::array rust_only{Language::rust};
constexpr std::array go_only{Language::go};
constexpr std::array cs_only{Language::cs};
constexpr std::array lua_only{Language::lua};
constexpr std::array shell_only{Language::shell};
constexpr std::array json_only{Language::json};
constexpr std::array css_only{Language::css};
constexpr std::array html_only{Language::html};
constexpr std::array yaml_only{Language::yaml};
constexpr std::array dockerfile_only{Language::dockerfile};
constexpr std::array php_only{Language::php};
constexpr std::array markdown_only{Language::markdown};

// 候補（探す順）。npm の物は包みの名前と bin の名前。
constexpr std::array clangd{LanguageServerCandidate{"clangd", "", ""}};
constexpr std::array python_servers{
    LanguageServerCandidate{"pyright-langserver", "pyright", "--stdio"},
    LanguageServerCandidate{"basedpyright-langserver", "", "--stdio"},
    LanguageServerCandidate{"basedpyright-langserver", "basedpyright", "--stdio"},
    LanguageServerCandidate{"pylsp", "", ""},
    LanguageServerCandidate{"jedi-language-server", "", ""}};
constexpr std::array typescript_server{
    LanguageServerCandidate{"typescript-language-server", "typescript-language-server", "--stdio"}};
constexpr std::array rust_analyzer{LanguageServerCandidate{"rust-analyzer", "", ""}};
constexpr std::array gopls{LanguageServerCandidate{"gopls", "", ""}};
constexpr std::array csharp_ls{LanguageServerCandidate{"csharp-ls", "", ""}};
constexpr std::array lua_server{LanguageServerCandidate{"lua-language-server", "", ""}};
constexpr std::array bash_server{
    LanguageServerCandidate{"bash-language-server", "bash-language-server", "start"}};
constexpr std::array json_server{LanguageServerCandidate{
    "vscode-json-language-server", "vscode-langservers-extracted", "--stdio"}};
constexpr std::array css_server{LanguageServerCandidate{"vscode-css-language-server",
                                                        "vscode-langservers-extracted", "--stdio"}};
constexpr std::array html_server{LanguageServerCandidate{
    "vscode-html-language-server", "vscode-langservers-extracted", "--stdio"}};
constexpr std::array yaml_server{
    LanguageServerCandidate{"yaml-language-server", "yaml-language-server", "--stdio"}};
constexpr std::array docker_server{
    LanguageServerCandidate{"docker-langserver", "dockerfile-language-server-nodejs", "--stdio"}};
constexpr std::array intelephense{
    LanguageServerCandidate{"intelephense", "intelephense", "--stdio"}};
constexpr std::array marksman{LanguageServerCandidate{"marksman", "", "server"}};

constexpr std::array table{
    KnownLanguageServer{"C / C++", c_family, clangd, "winget install LLVM.LLVM"},
    KnownLanguageServer{"Python", python_only, python_servers, "npm i -g pyright"},
    KnownLanguageServer{"TypeScript / JavaScript", script_family, typescript_server,
                        "npm i -g typescript-language-server typescript"},
    KnownLanguageServer{"Rust", rust_only, rust_analyzer, "rustup component add rust-analyzer"},
    KnownLanguageServer{"Go", go_only, gopls, "go install golang.org/x/tools/gopls@latest"},
    KnownLanguageServer{"C#", cs_only, csharp_ls, "dotnet tool install -g csharp-ls"},
    KnownLanguageServer{"Lua", lua_only, lua_server, ""},
    KnownLanguageServer{"Shell", shell_only, bash_server, "npm i -g bash-language-server"},
    KnownLanguageServer{"JSON", json_only, json_server, "npm i -g vscode-langservers-extracted"},
    KnownLanguageServer{"CSS", css_only, css_server, "npm i -g vscode-langservers-extracted"},
    KnownLanguageServer{"HTML", html_only, html_server, "npm i -g vscode-langservers-extracted"},
    KnownLanguageServer{"YAML", yaml_only, yaml_server, "npm i -g yaml-language-server"},
    KnownLanguageServer{"Dockerfile", dockerfile_only, docker_server,
                        "npm i -g dockerfile-language-server-nodejs"},
    KnownLanguageServer{"PHP", php_only, intelephense, "npm i -g intelephense"},
    KnownLanguageServer{"Markdown", markdown_only, marksman, ""}};

// 1 つの言語が 2 つの行に現れない（「使う」が同じ命令の行を 2 つの行から書き合わない）。
[[nodiscard]] consteval bool languages_listed_once()
{
    std::array<int, 64> seen{};
    for (const auto &row : table)
    {
        for (const Language language : row.languages)
        {
            ++seen.at(std::to_underlying(language));
        }
    }
    return std::ranges::all_of(seen, [](int count) { return count <= 1; }) &&
           std::ranges::none_of(table, [](const KnownLanguageServer &row)
                                { return row.languages.empty() || row.candidates.empty(); });
}
static_assert(languages_listed_once(), "every language appears in at most one row");
static_assert(known_language_servers_leading <= table.size(), "the leading rows are in the table");

// 空白（SP・HT）か。
[[nodiscard]] constexpr bool blank(char character) noexcept
{
    return character == ' ' || character == '\t';
}

[[nodiscard]] bool has_blank(std::string_view text) noexcept
{
    for (const char character : text)
    {
        if (blank(character))
        {
            return true;
        }
    }
    return false;
}

// 命令の行に書けない文字（`"` は語の囲み・改行は設定の行を割る・制御文字）。
[[nodiscard]] bool unwritable(std::string_view text) noexcept
{
    for (const char character : text)
    {
        if (character == '"' || static_cast<unsigned char>(character) < 0x20U)
        {
            return true;
        }
    }
    return false;
}

void append_word(std::string &line, std::string_view word)
{
    if (!line.empty())
    {
        line.push_back(' ');
    }
    if (has_blank(word))
    {
        line.push_back('"');
        line.append(word);
        line.push_back('"');
        return;
    }
    line.append(word);
}

// `@scope/name` の name の所。
[[nodiscard]] std::string_view unscoped(std::string_view package) noexcept
{
    if (package.empty() || package.front() != '@')
    {
        return package;
    }
    for (std::size_t at = 0; at < package.size(); ++at)
    {
        if (package.at(at) == '/')
        {
            return package.substr(at + 1);
        }
    }
    return package;
}

// 区切り（`/` か `\`）の前までの長さ。
[[nodiscard]] std::size_t segment_length(std::string_view rest) noexcept
{
    std::size_t end = 0;
    while (end < rest.size() && rest.at(end) != '/' && rest.at(end) != '\\')
    {
        ++end;
    }
    return end;
}

// 包みの外を指す段（`..`・ドライブの `:`）。
[[nodiscard]] bool escaping(std::string_view segment) noexcept
{
    return segment == ".." || std::ranges::find(segment, ':') != segment.end();
}

// 空か、区切りで始まる（絶対パスか UNC）。
[[nodiscard]] bool rooted(std::string_view path) noexcept
{
    return path.empty() || path.front() == '/' || path.front() == '\\';
}

// 段を `\` でつなぐ（空の段と `.` は落とす）。
void append_segment(std::string &joined, std::string_view segment)
{
    if (segment.empty() || segment == ".")
    {
        return;
    }
    joined += joined.empty() ? "" : "\\";
    joined.append(segment);
}

// 包みのフォルダからの相対パスを `\` の区切りに直す。段の `.` と空の段は落とし、`..`・先頭の
// 区切り・ドライブ・`"`・制御文字は断る。
[[nodiscard]] std::optional<std::string> contained_relative(std::string_view path)
{
    if (rooted(path) || unwritable(path))
    {
        return std::nullopt;
    }
    std::string joined;
    for (std::string_view rest = path; !rest.empty();)
    {
        const std::size_t end = segment_length(rest);
        const std::string_view segment = rest.substr(0, end);
        rest.remove_prefix(end == rest.size() ? end : end + 1);
        if (escaping(segment))
        {
            return std::nullopt;
        }
        append_segment(joined, segment);
    }
    return joined.empty() ? std::nullopt : std::optional{joined};
}
} // namespace

std::span<const KnownLanguageServer> known_language_servers() noexcept
{
    return table;
}

std::optional<std::string> npm_bin_script(std::string_view manifest, std::string_view package,
                                          std::string_view bin)
{
    const auto parsed = parse_json(manifest);
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    const JsonObject *object = json_object_of(parsed.value());
    if (object == nullptr)
    {
        return std::nullopt;
    }
    const auto entry = json_member(*object, "bin");
    if (!entry.has_value())
    {
        return std::nullopt;
    }
    const auto &value = entry.value().get().value;
    if (const auto *single = std::get_if<std::string>(&value))
    {
        return unscoped(package) == bin ? contained_relative(*single) : std::nullopt;
    }
    const auto *bins = std::get_if<JsonObject>(&value);
    const std::string *script = bins != nullptr ? json_text_member(*bins, bin) : nullptr;
    return script != nullptr ? contained_relative(*script) : std::nullopt;
}

std::optional<std::string> language_server_command_text(std::string_view executable,
                                                        std::string_view script,
                                                        std::string_view arguments)
{
    if (executable.empty() || unwritable(executable) || unwritable(script) || unwritable(arguments))
    {
        return std::nullopt;
    }
    std::string line;
    append_word(line, executable);
    if (!script.empty())
    {
        append_word(line, script);
    }
    if (!arguments.empty())
    {
        line.push_back(' ');
        line.append(arguments);
    }
    return line;
}
} // namespace nenenib::core
