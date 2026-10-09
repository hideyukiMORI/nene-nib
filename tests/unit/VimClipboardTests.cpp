// scope `--vim-clipboard` の単体テスト（ADR 0042 決定 2・ADR 0051 決定 9）。`"+` `"*` の読み書きを
// controller に通し、OS の代わりに替え玉の ScriptedClipboard だけを読み書きする。oracle は実機の
// クリップボードを書き換えるので fixture にできない。期待値は本物の Vim 9.1 の実測
// （out/probes/probe-clipboard-2026-09-29.md の B 節）で、OS へ出す改行は Ctrl+C と同じ文書の形。
#include "ClipboardFailure.hpp"
#include "Editing.hpp"
#include "Scopes.hpp"
#include "ScriptedClipboard.hpp"
#include "TestSupport.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <tuple>

namespace nenenib::tests
{
namespace
{
using nenenib::core::VimRegister;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimState;

// OS の本文・写しの本文・写しの種類（決定 4・B3）。`"+3` の途中で写しが残る間に読む。
using ClipboardRead = std::tuple<std::string_view, std::string_view, VimRegisterKind>;

constexpr std::array<ClipboardRead, 9> clipboard_reads{{
    {"abc", "abc", VimRegisterKind::characters},
    {"abc\r\n", "abc\n", VimRegisterKind::lines},
    {"abc\n", "abc\n", VimRegisterKind::lines},
    {"ab\r\ncd", "ab\ncd", VimRegisterKind::characters},
    {"ab\r\ncd\r\n", "ab\ncd\n", VimRegisterKind::lines},
    {"ab\rcd", "ab\rcd", VimRegisterKind::characters},
    {"abc\r\n\r\n", "abc\n\n", VimRegisterKind::lines},
    {"\r\n", "\n", VimRegisterKind::lines},
    {"", "", VimRegisterKind::characters},
}};

// OS の本文・鍵・貼った後の本文（B3・B4）。文書は X1 / X2 の 2 行で、キャレットは 1 行 1 桁。
using ClipboardPut = std::tuple<std::string_view, std::string_view, std::string_view>;

constexpr std::array<ClipboardPut, 13> clipboard_puts{{
    {"abc", "\"+p", "Xabc1\nX2"},
    {"abc\r\n", "\"+p", "X1\nabc\nX2"},
    {"abc\n", "\"+p", "X1\nabc\nX2"},
    {"ab\r\ncd", "\"+p", "Xab\ncd1\nX2"},
    {"ab\r\ncd\r\n", "\"+p", "X1\nab\ncd\nX2"},
    {"ab\rcd", "\"+p", "Xab\rcd1\nX2"},
    {"abc\r\n\r\n", "\"+p", "X1\nabc\n\nX2"},
    {"\r\n", "\"+p", "X1\n\nX2"},
    {"abc\r\n", "\"*p", "X1\nabc\nX2"},
    {"ab\r\ncd", "\"*p", "Xab\ncd1\nX2"},
    {"abc", "3\"+p", "Xabcabcabc1\nX2"},
    {"abc", "\"+3p", "Xabcabcabc1\nX2"},
    {"abc", "\"+P", "abcX1\nX2"},
}};

// 鍵・文書（LF）・OS の本文（LF 文書）・OS の本文（CRLF 文書）・種類（B1・B2）。無名は engine の
// 本文（LF）のまま。CRLF 文書は文書の LF を CRLF に直して開く。
using ClipboardWrite = std::tuple<std::string_view, std::string_view, std::string_view,
                                  std::string_view, VimRegisterKind>;

constexpr std::array<ClipboardWrite, 8> clipboard_writes{{
    {"\"+yy", "one\ntwo", "one\n", "one\r\n", VimRegisterKind::lines},
    {"\"*yy", "one\ntwo", "one\n", "one\r\n", VimRegisterKind::lines},
    {"\"+yj", "one\ntwo", "one\ntwo\n", "one\r\ntwo\r\n", VimRegisterKind::lines},
    {"\"+yw", "hello world", "hello ", "hello ", VimRegisterKind::characters},
    {"\"+dd", "one\ntwo", "one\n", "one\r\n", VimRegisterKind::lines},
    {"\"*dd", "one\ntwo", "one\n", "one\r\n", VimRegisterKind::lines},
    {"\"+x", "one\ntwo", "o", "o", VimRegisterKind::characters},
    {"vj\"+y", "one\ntwo", "one\nt", "one\r\nt", VimRegisterKind::characters},
}};

[[nodiscard]] Content held(std::string_view text)
{
    return Content{std::string(text)};
}

[[nodiscard]] bool unused(const VimRegister &value) noexcept
{
    return value.kind == VimRegisterKind::uninitialized;
}

[[nodiscard]] const VimRegister &numbered(const VimState &state, std::size_t index)
{
    return state.numbered.registers.at(index).value();
}

[[nodiscard]] std::string crlf_of(std::string_view text)
{
    std::string result;
    for (const char byte : text)
    {
        if (byte == '\n')
        {
            result += "\r\n";
            continue;
        }
        result.push_back(byte);
    }
    return result;
}

// 替え玉に置かれた本文。読むと読まれた回数が 1 つ進むので、回数を見た後にだけ呼ぶ。
[[nodiscard]] std::string written_text(Editing &editing)
{
    return editing.clipboard().read().value_or(std::string{"<none>"});
}

// 読みの表（決定 3・4）。`"+` を選んだ後の `3` の直前に読み、回数の途中は写しが残るのでそこで見る。
void verify_clipboard_read_table()
{
    for (const auto &[os_text, text, kind] : clipboard_reads)
    {
        Editing editing;
        open_vim_document(editing, "X1\nX2");
        editing.clipboard().hold(held(os_text));
        vim_replay(editing.controller(), "\"+3");
        const VimRegister &copy = editing.controller().vim_state().clipboard.value();
        expect(copy.text == text && copy.kind == kind,
               "the clipboard text folds CRLF and a trailing newline makes it linewise");
        expect(editing.clipboard().reads() == 1, "\"+3 reads the clipboard right before 3");
        vim_replay(editing.controller(), "<Esc>");
        expect(unused(editing.controller().vim_state().clipboard.value()), "Esc drops the copy");
    }
}

// `"+p` `"*p` `3"+p` `"+3p` `"+P`（決定 3〜5・B3・B4）。貼っても OS へは書かず、写しは消える。
void verify_clipboard_put()
{
    for (const auto &[os_text, keys, expected] : clipboard_puts)
    {
        Editing editing;
        open_vim_document(editing, "X1\nX2");
        editing.clipboard().hold(held(os_text));
        vim_replay(editing.controller(), keys);
        expect(whole_vim_body(editing.controller()) == expected,
               "\"+p pastes the clipboard text as read");
        expect(editing.clipboard().writes() == 0, "\"+p does not write the clipboard");
        expect(unused(editing.controller().vim_state().clipboard.value()),
               "the copy is gone after the command");
    }
}

// 普通の打鍵では読まず、`"+` を選んでいる間は鍵ごとに 1 回読む（決定 3・ADR の結果の節）。
void verify_clipboard_read_count()
{
    Editing plain;
    open_vim_document(plain, "one\ntwo");
    vim_replay(plain.controller(), "yyjp");
    expect(plain.clipboard().reads() == 0 && plain.clipboard().writes() == 0,
           "keys without \"+ never touch the clipboard");
    Editing yank;
    open_vim_document(yank, "one\ntwo");
    vim_replay(yank.controller(), "\"+yy");
    expect(yank.clipboard().reads() == 2 && yank.clipboard().writes() == 1,
           "\"+yy reads before y and before the second y and writes once");
    Editing put;
    open_vim_document(put, "one\ntwo");
    put.clipboard().hold(held("abc"));
    vim_replay(put.controller(), "\"+p");
    expect(put.clipboard().reads() == 1, "\"+p reads once, right before p");
}

// `"+yy` `"+yw` `"+dd` `"+x` ほか（決定 6・7・B1・B2）。OS へは文書の改行の形で 1 回書き、無名は
// engine の本文（LF）と種類のまま。
void verify_clipboard_written_text()
{
    for (const auto &[keys, text, lf, crlf, kind] : clipboard_writes)
    {
        Editing unix_like;
        open_vim_document(unix_like, std::string(text));
        vim_replay(unix_like.controller(), keys);
        const VimRegister &unnamed = unix_like.controller().vim_state().unnamed_register.value();
        expect(unnamed.text == lf && unnamed.kind == kind,
               "the unnamed register takes what \"+ takes");
        expect(unix_like.clipboard().writes() == 1, "\"+ writes the clipboard once");
        expect(written_text(unix_like) == lf, "an LF document puts LF on the clipboard");
        expect(unused(unix_like.controller().vim_state().clipboard.value()),
               "writing does not leave a copy in the state");
        Editing windows_like;
        open_vim_document(windows_like, crlf_of(text));
        vim_replay(windows_like.controller(), keys);
        expect(windows_like.clipboard().writes() == 1 && written_text(windows_like) == crlf,
               "a CRLF document puts CRLF on the clipboard");
    }
}

// `"0` `"1` `"-` の規則は名前つきと同じ（決定 6・ADR 0050 の決定 3・B1）。
void verify_clipboard_written_registers()
{
    Editing yank;
    open_vim_document(yank, "one\ntwo");
    vim_replay(yank.controller(), "\"+yy");
    expect(unused(numbered(yank.controller().vim_state(), 0)), "\"+yy leaves \"0 alone");
    Editing line;
    open_vim_document(line, "one\ntwo\nthree");
    vim_replay(line.controller(), "\"+dd");
    const VimState &once = line.controller().vim_state();
    expect(numbered(once, 1).text == "one\n" && unused(numbered(once, 0)) &&
               unused(once.small_delete.value()),
           "\"+dd fills \"1 and leaves \"0 and \"- alone");
    vim_replay(line.controller(), "\"+dd");
    const VimState &twice = line.controller().vim_state();
    expect(numbered(twice, 1).text == "two\n" && numbered(twice, 2).text == "one\n",
           "a second \"+dd shifts \"1 into \"2");
    expect(line.clipboard().writes() == 2 && written_text(line) == "two\n",
           "each \"+dd writes the clipboard");
    Editing character;
    open_vim_document(character, "one\ntwo");
    vim_replay(character.controller(), "\"+x");
    const VimState &small = character.controller().vim_state();
    expect(unused(numbered(small, 1)) && unused(small.small_delete.value()),
           "\"+x inside a line leaves \"1 and \"- alone");
}

// 書けなくても本文・無名・`"1` は engine の結果のとおり（決定 7）。
void verify_clipboard_write_refused()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    editing.clipboard().refuse_writes();
    vim_replay(editing.controller(), "\"+dd");
    const VimState &state = editing.controller().vim_state();
    expect(whole_vim_body(editing.controller()) == "two", "a refused write still deletes");
    expect(state.unnamed_register.value().text == "one\n" && numbered(state, 1).text == "one\n",
           "a refused write still fills the unnamed register and \"1");
    expect(editing.clipboard().writes() == 1, "the refused write was tried once");
}

// 読めない・空文字列の `"+p` は空のレジスタと同じに拒まれ、本文は変わらない（決定 3・5・B6）。
void verify_clipboard_put_refused()
{
    const std::array<Content, 3> contents{
        Content{std::unexpected(ClipboardFailure::empty)},
        Content{std::unexpected(ClipboardFailure::unsupported_format)},
        held(""),
    };
    for (const Content &content : contents)
    {
        Editing editing;
        open_vim_document(editing, "X1\nX2");
        editing.clipboard().hold(content);
        vim_replay(editing.controller(), "\"+p");
        expect(whole_vim_body(editing.controller()) == "X1\nX2",
               "\"+p of an unreadable or empty clipboard changes nothing");
        expect(editing.clipboard().writes() == 0, "a refused \"+p writes nothing");
    }
}

// `.` は再生の時点のクリップボードを読み直す（決定 3・8・B5）。本文も種類も新しい方。
void verify_clipboard_dot()
{
    Editing characters;
    open_vim_document(characters, "X1");
    characters.clipboard().hold(held("OLD"));
    vim_replay(characters.controller(), "\"+p");
    characters.clipboard().hold(held("NEW"));
    vim_replay(characters.controller(), ".");
    expect(whole_vim_body(characters.controller()) == "XOLDNEW1",
           ". after \"+p pastes the clipboard as it is now");
    Editing lines;
    open_vim_document(lines, "X1");
    lines.clipboard().hold(held("OLD\r\n"));
    vim_replay(lines.controller(), "\"+p");
    lines.clipboard().hold(held("NEW"));
    vim_replay(lines.controller(), ".");
    expect(whole_vim_body(lines.controller()) == "X1\nONEWLD",
           ". after a linewise \"+p pastes the new text with its new kind");
    expect(lines.clipboard().writes() == 0, ". of \"+p writes nothing");
}

// `@+` `@*` と `@@`（決定 5・B8）。`@@` も再生の時点で読み直す。
void verify_clipboard_macro_replay()
{
    Editing plus;
    open_vim_document(plus, "abc");
    plus.clipboard().hold(held("x"));
    vim_replay(plus.controller(), "@+");
    expect(whole_vim_body(plus.controller()) == "bc", "@+ runs the clipboard text");
    plus.clipboard().hold(held("lx"));
    vim_replay(plus.controller(), "@@");
    expect(whole_vim_body(plus.controller()) == "b", "@@ after @+ reads the clipboard again");
    Editing star;
    open_vim_document(star, "abc");
    star.clipboard().hold(held("x\r\n"));
    vim_replay(star.controller(), "@*");
    expect(whole_vim_body(star.controller()) == "bc", "@* runs a linewise clipboard text");
    expect(plus.clipboard().writes() == 0 && star.clipboard().writes() == 0,
           "@+ and @* write nothing");
}

// マクロの中の `"+p` も再生の時点で読む（決定 3）。`"ayy` の本文 `"+p<NL>` を `@a` で走らせる。
void verify_clipboard_inside_macro()
{
    Editing editing;
    open_vim_document(editing, "\"+p\nsecond");
    editing.clipboard().hold(held("Z"));
    vim_replay(editing.controller(), "\"ayy@a");
    expect(whole_vim_body(editing.controller()) == "\"Z+p\nsecond",
           "\"+p inside @a pastes the clipboard");
    expect(caret_at(editing.controller().frame(), 2, 2), "the <NL> of the macro moves down");
}

// 矩形の `"+y` は行を文書の改行で繋いだ本文になり、続く `"+p` は矩形でない
// （決定 7・結果の節）。
void verify_clipboard_block()
{
    Editing unix_like;
    open_vim_document(unix_like, "abcd\nefgh\nijkl");
    vim_replay(unix_like.controller(), "<C-v>jl\"+y");
    expect(unix_like.clipboard().writes() == 1 && written_text(unix_like) == "ab\nef",
           "a block \"+y writes its rows joined with LF in an LF document");
    expect(unix_like.controller().vim_state().unnamed_register.value().kind ==
               VimRegisterKind::block,
           "the unnamed register keeps the block");
    vim_replay(unix_like.controller(), "\"+p");
    expect(whole_vim_body(unix_like.controller()) == "aab\nefbcd\nefgh\nijkl",
           "\"+p of a block yank pastes characters, not a block");
    Editing windows_like;
    open_vim_document(windows_like, "abcd\r\nefgh\r\nijkl");
    vim_replay(windows_like.controller(), "<C-v>jl\"+y");
    expect(written_text(windows_like) == "ab\r\nef", "a block joins its rows with CRLF");
}
} // namespace

void verify_vim_clipboard_contracts()
{
    verify_clipboard_read_table();
    verify_clipboard_put();
    verify_clipboard_read_count();
    verify_clipboard_written_text();
    verify_clipboard_written_registers();
    verify_clipboard_write_refused();
    verify_clipboard_put_refused();
    verify_clipboard_dot();
    verify_clipboard_macro_replay();
    verify_clipboard_inside_macro();
    verify_clipboard_block();
}

void verify_vim_clipboard_scope()
{
    verify_vim_clipboard_contracts();
}
} // namespace nenenib::tests
