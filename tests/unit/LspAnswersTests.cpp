// scope `--lsp-answers` の契約（#1825・#1821 の段 A・ADR 0190 の決定 4・11）。core の答えの読み
// 3 本（補完・hover・定義へ）とスニペットの印・URI → パス・percent-encoding の表の試験。
#include "DocumentUri.hpp"
#include "FilePath.hpp"
#include "FileUri.hpp"
#include "JsonNull.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "LspAnswerFailure.hpp"
#include "LspAnswers.hpp"
#include "LspCompletionItem.hpp"
#include "LspCompletionList.hpp"
#include "LspLocation.hpp"
#include "LspPosition.hpp"
#include "LspRange.hpp"
#include "LspSnippet.hpp"
#include "PercentDecoding.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::LspAnswerFailure;
using core::LspPosition;
using core::LspRange;

[[nodiscard]] core::JsonValue json(std::string_view text)
{
    auto parsed = core::parse_json(text);
    expect(parsed.has_value(), "the test JSON parses");
    return parsed.has_value() ? std::move(parsed).value() : core::JsonValue{core::JsonNull{}};
}

// 答えの形でないと断ったか。
template <typename Value>
[[nodiscard]] bool refused(const std::expected<Value, LspAnswerFailure> &answer)
{
    return !answer.has_value() && answer.error() == LspAnswerFailure::invalid_result;
}

[[nodiscard]] LspRange range_of(std::size_t line, std::size_t start, std::size_t end)
{
    return LspRange{.start = LspPosition{.line = line, .character = start},
                    .end = LspPosition{.line = line, .character = end}};
}

[[nodiscard]] std::optional<std::string> path_text(std::string_view uri)
{
    const auto path = core::file_path_of_uri(uri);
    return path.has_value() ? std::optional<std::string>{std::string{path.value().text()}}
                            : std::nullopt;
}

// CompletionItem[] と CompletionList（決定 4）。label の無い項目は捨て、欄の無い所は label。
void verify_completion_shapes()
{
    const auto items = core::lsp_completion_items(json(
        R"j([{"label":"foo","kind":3,"detail":"int foo()"},{"label":"bar","insertText":"bar()"},{"nolabel":1}])j"));
    expect(items.has_value() && !items.value().incomplete && items.value().items.size() == 2,
           "a CompletionItem array keeps the labelled items in order and is complete");
    const auto &foo = items.value().items.at(0);
    expect(foo.label == "foo" && foo.kind == std::optional<std::uint64_t>{3} &&
               foo.detail == "int foo()" && foo.sort_text == "foo" && foo.filter_text == "foo" &&
               foo.insert_text == "foo" && !foo.edit_range.has_value(),
           "a bare item inserts, sorts and filters by its label");
    const auto &bar = items.value().items.at(1);
    expect(bar.insert_text == "bar()" && !bar.kind.has_value() && bar.detail.empty(),
           "insertText is the inserted text and missing fields stay empty");
    const auto list = core::lsp_completion_items(json(
        R"({"isIncomplete":true,"items":[{"label":"a","sortText":"0001","filterText":"aa"}]})"));
    expect(list.has_value() && list.value().incomplete && list.value().items.size() == 1 &&
               list.value().items.front().sort_text == "0001" &&
               list.value().items.front().filter_text == "aa",
           "a CompletionList keeps isIncomplete, sortText and filterText");
    const auto empty = core::lsp_completion_items(json("null"));
    expect(empty.has_value() && empty.value().items.empty() && !empty.value().incomplete,
           "null is an empty complete list");
    expect(refused(core::lsp_completion_items(json(R"("x")"))) &&
               refused(core::lsp_completion_items(json(R"({"isIncomplete":false})"))),
           "a string or an object without items is not a completion answer");
}

// textEdit の TextEdit と InsertReplaceEdit（insert の範囲）・崩れた textEdit（決定 4）。
void verify_completion_edits()
{
    const auto items = core::lsp_completion_items(json(
        R"([{"label":"x","insertText":"ignored","textEdit":{"range":{"start":{"line":0,"character":1},"end":{"line":0,"character":3}},"newText":"xyz"}},)"
        R"({"label":"y","textEdit":{"insert":{"start":{"line":2,"character":1},"end":{"line":2,"character":2}},"replace":{"start":{"line":2,"character":1},"end":{"line":2,"character":5}},"newText":"yy"}},)"
        R"({"label":"z","insertText":"zz","textEdit":{"newText":"broken"}}])"));
    expect(items.has_value() && items.value().items.size() == 3, "all three items are kept");
    const auto &x = items.value().items.at(0);
    expect(x.insert_text == "xyz" && x.edit_range == std::optional{range_of(0, 1, 3)},
           "a TextEdit gives its newText and range over insertText");
    const auto &y = items.value().items.at(1);
    expect(y.insert_text == "yy" && y.edit_range == std::optional{range_of(2, 1, 2)},
           "an InsertReplaceEdit uses the insert range");
    const auto &z = items.value().items.at(2);
    expect(z.insert_text == "zz" && !z.edit_range.has_value(),
           "a textEdit without a range is ignored");
}

// スニペット形式の印を外す（決定 4）と、上限 500 件。
void verify_completion_snippets_and_limit()
{
    const auto items = core::lsp_completion_items(json(
        R"([{"label":"for","insertTextFormat":2,"insertText":"for (${1:int i} = 0; $2) {\n\t$0\n}"},)"
        R"({"label":"plain","insertTextFormat":1,"insertText":"a$1"}])"));
    expect(items.has_value() &&
               items.value().items.at(0).insert_text == "for (int i = 0; ) {\n\t\n}",
           "a snippet item inserts the text without its tab stops");
    expect(items.value().items.at(1).insert_text == "a$1", "a plain text item keeps its dollars");
    expect(core::lsp_snippet_text("${1|one,two|}") == "one" &&
               core::lsp_snippet_text(R"(\$1 \} \\)") == R"($1 } \)" &&
               core::lsp_snippet_text("${1:a${2:b}c}d") == "abcd" &&
               core::lsp_snippet_text("$TM_SELECTED_TEXT${NAME}x") == "x" &&
               core::lsp_snippet_text("${VAR:fallback}") == "fallback" &&
               core::lsp_snippet_text("cost $ and ${ and ${1") == "cost $ and ${ and ${1" &&
               core::lsp_snippet_text("{a}") == "{a}",
           "choices, escapes, nesting and variables lose their marks and stray marks stay");
    // 表（レビュー R3）: `$数字` は数字だけを読む。
    using SnippetRow = std::pair<std::string_view, std::string_view>;
    for (const auto &[snippet, text] : std::vector<SnippetRow>{{"$1abc", "abc"},
                                                               {"${1:x}", "x"},
                                                               {"${1|a,b|}", "a"},
                                                               {"$0", ""},
                                                               {"a$12b", "ab"},
                                                               {R"(\$)", "$"},
                                                               {"$name", ""},
                                                               {"$_x1 y", " y"}})
    {
        expect(core::lsp_snippet_text(snippet) == text, "one snippet row loses exactly its marks");
    }
    std::string many = "[";
    for (std::size_t index = 0; index <= core::lsp_completion_limit; ++index)
    {
        many += index == 0 ? R"({"label":"w)" : R"(,{"label":"w)";
        many += std::to_string(index) + R"("})";
    }
    const auto limited = core::lsp_completion_items(json(many + "]"));
    expect(limited.has_value() && limited.value().items.size() == core::lsp_completion_limit &&
               limited.value().items.back().label == "w499" && limited.value().incomplete,
           "an answer over the limit keeps the first 500 items and asks to be asked again");
}

// hover の 4 つの形と囲みの行・改行・上限（決定 4）。
void verify_hover_shapes()
{
    expect(
        core::lsp_hover_text(json(
            R"({"contents":{"kind":"markdown","value":"```cpp\nint foo()\n```\n\nReturns 1."}})")) ==
            std::optional<std::string>{"int foo()\n\nReturns 1."},
        "markdown MarkupContent drops the fence lines");
    expect(core::lsp_hover_text(
               json(R"({"contents":{"kind":"plaintext","value":"\r\na\r\n```b\r\n\r\n"}})")) ==
               std::optional<std::string>{"a\n```b"},
           "plaintext keeps backticks, uses LF and drops the blank lines around it");
    expect(core::lsp_hover_text(json(R"({"contents":"**bold**"})")) ==
               std::optional<std::string>{"**bold**"},
           "a MarkedString string is markdown kept as it is");
    expect(
        core::lsp_hover_text(json(
            R"({"contents":[{"language":"cpp","value":"int x"},"doc"],"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}})")) ==
            std::optional<std::string>{"int x\ndoc"},
        "a MarkedString array is joined line by line");
    expect(!core::lsp_hover_text(json("null")).has_value() &&
               !core::lsp_hover_text(json(R"({"contents":""})")).has_value() &&
               !core::lsp_hover_text(json(R"({"contents":"```\n```"})")).has_value() &&
               !core::lsp_hover_text(json(R"({"contents":[]})")).has_value() &&
               !core::lsp_hover_text(json(R"({"range":{}})")).has_value(),
           "null, empty and fence-only hovers are no text");
}

void verify_hover_limits()
{
    std::string lines;
    for (std::size_t index = 0; index < 50; ++index)
    {
        lines += "line" + std::to_string(index) + "\\n";
    }
    const auto cut = core::lsp_hover_text(json(R"({"contents":")" + lines + R"("})"));
    std::size_t breaks = 0;
    for (const char character : cut.value_or(""))
    {
        breaks += character == '\n' ? 1U : 0U;
    }
    expect(cut.has_value() && breaks + 1U == core::lsp_hover_line_limit &&
               cut.value().ends_with("line39"),
           "a hover over 40 lines keeps the first 40");
    std::string wide;
    for (std::size_t index = 0; index < 2000; ++index)
    {
        wide += "あ";
    }
    const auto bytes = core::lsp_hover_text(json(R"({"contents":")" + wide + R"("})"));
    expect(bytes.has_value() && bytes.value().size() == 4095 && bytes.value().size() % 3 == 0,
           "a hover over 4 KiB is cut at a code point boundary");
}

// 定義の答え（決定 4）。Location・Location[]・LocationLink[]・null。
void verify_locations()
{
    const std::string range =
        R"({"start":{"line":3,"character":4},"end":{"line":3,"character":7}})";
    const auto single =
        core::lsp_locations(json(R"({"uri":"file:///C:/a.cpp","range":)" + range + "}"));
    expect(single.has_value() && single.value().size() == 1 &&
               single.value().front().uri == "file:///C:/a.cpp" &&
               single.value().front().range == range_of(3, 4, 7),
           "a Location is one place");
    const auto listed = core::lsp_locations(
        json(R"([{"uri":"file:///C:/a.cpp","range":)" + range +
             R"(},{"uri":"file:///C:/b.cpp"},{"uri":"file:///C:/c.cpp","range":)" + range + "}]"));
    expect(listed.has_value() && listed.value().size() == 2 &&
               listed.value().back().uri == "file:///C:/c.cpp",
           "a Location array keeps the whole places in order");
    const auto links = core::lsp_locations(json(
        R"([{"targetUri":"file:///C:/d.cpp","targetRange":{"start":{"line":1,"character":0},"end":{"line":5,"character":1}},"targetSelectionRange":)" +
        range +
        R"(},{"targetUri":"file:///C:/e.cpp","targetRange":{"start":{"line":9,"character":2},"end":{"line":9,"character":3}}}])"));
    expect(links.has_value() && links.value().size() == 2 &&
               links.value().front().range == range_of(3, 4, 7) &&
               links.value().back().range == range_of(9, 2, 3),
           "a LocationLink uses the selection range and falls back to the target range");
    const auto none = core::lsp_locations(json("null"));
    expect(none.has_value() && none.value().empty(), "null is no place");
    expect(refused(core::lsp_locations(json("3"))), "a number is not a definition answer");
}

// 表（レビュー M1 / R1）: 値なしは std::nullopt。
void verify_file_uri_rows()
{
    using Row = std::pair<std::string_view, std::optional<std::string_view>>;
    for (const auto &[uri, path] :
         std::vector<Row>{{"file:///C:foo.cpp", std::nullopt},
                          {"file:///C:", std::nullopt},
                          {"file:///C:/", R"(C:\)"},
                          {"file://./pipe/x", std::nullopt},
                          {"file://%3F/C:/x", std::nullopt},
                          {"file://%2E/x/y", std::nullopt},
                          {"file://C:/x", std::nullopt},
                          {"file://server", std::nullopt},
                          {"file://server/", std::nullopt},
                          {"file://a%2Fb/s/x", std::nullopt},
                          {"file:///C:/a/../b.cpp", R"(C:\b.cpp)"},
                          {"file:///C:/a/./b//c", R"(C:\a\b\c)"},
                          {"file:///C:/../x", std::nullopt},
                          {"file://server/share/a/../b", R"(\\server\share\b)"},
                          {"file://server/share/a/../../x", std::nullopt},
                          {"file://server/../x", std::nullopt},
                          {"file:///C:/a.cpp#L3", R"(C:\a.cpp)"},
                          {"file:///C:/a.cpp?x=1", R"(C:\a.cpp)"},
                          {"file:///c:/Users/x/a.cpp", R"(C:\Users\x\a.cpp)"},
                          {"file:///C:/a%2Fb%5Cc", R"(C:\a\b\c)"},
                          {"file:///C:/a%00b", std::nullopt},
                          {"file:///C:/a%C3", std::nullopt},
                          {"file:///%5C%5C.%5Cpipe%5Cx", std::nullopt}})
    {
        const auto found = path_text(uri);
        expect(path.has_value() ? found == std::optional<std::string>{std::string{path.value()}}
                                : !found.has_value(),
               "one file URI row maps to its absolute path or is refused");
    }
}

// URI → パス（決定 4）と document_uri の往復・percent-encoding の 1 本。
void verify_file_uris()
{
    expect(path_text("file:///C:/a%20b/c.cpp") == std::optional<std::string>{"C:\\a b\\c.cpp"},
           "a drive URI decodes its spaces and uses backslashes");
    expect(path_text("file://server/share/x") == std::optional<std::string>{"\\\\server\\share\\x"},
           "a URI with a host is a UNC path");
    expect(path_text("FILE:///c%3A/x.cpp") == std::optional<std::string>{"C:\\x.cpp"} &&
               path_text("file://localhost/C:/x") == std::optional<std::string>{"C:\\x"} &&
               path_text("file:/C:/y") == std::optional<std::string>{"C:\\y"},
           "the scheme ignores case, the colon may be encoded, the drive is upper case and "
           "localhost is no host");
    verify_file_uri_rows();
    expect(!path_text("http://host/x").has_value() &&
               !path_text("untitled:Untitled-1").has_value() &&
               !path_text("file:///usr/include/x.h").has_value() &&
               !path_text("file:///C:/a%00b").has_value() &&
               !path_text("file:///C:/%FF").has_value() && !path_text("fil").has_value(),
           "other schemes, paths without a drive, control bytes and broken UTF-8 are refused");
    for (const std::string_view text :
         {std::string_view{"C:\\a b\\日本 #1%.cpp"}, std::string_view{"\\\\host\\share\\x y.txt"}})
    {
        const auto path = core::FilePath::parse(text).value();
        expect(core::file_path_of_uri(application::document_uri(path)) == std::optional{path},
               "document_uri and file_path_of_uri go back and forth");
    }
    expect(core::percent_decoded("%41%zz%4") == "A%zz%4" &&
               core::percent_decoded("a%2f%2F") == "a//",
           "percent decoding turns two hex digits into a byte and leaves the rest");
}
} // namespace

void verify_lsp_answers_scope()
{
    verify_completion_shapes();
    verify_completion_edits();
    verify_completion_snippets_and_limit();
    verify_hover_shapes();
    verify_hover_limits();
    verify_locations();
    verify_file_uris();
}
} // namespace nenenib::tests
