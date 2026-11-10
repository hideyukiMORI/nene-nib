// scope `--vim-characters` の契約のうち、窓の道で打った本文（#1309 の差し戻し・ADR 0053 の追補
// #1309）。窓は Vim の鍵を 1 つずつ入口の意図 VimKeyPress で送り、入口の終わりの寄せを通る。
// fixture は入口を通らない press_vim_key で再生するので、入口の処理が Vim と違っても fixture では
// 落ちない。そこで (1) 窓の道で打った本文を Vim 9.1 の実測と比べる表と、(2) 窓の道と fixture の道が
// 同じ本文・キャレット・モードになる固定 seed の小さな性質の 2 つで守る。実測の probe は
// `D:\NeNeNib\scripts\20261110\1309-fix\probe.vim`（`-u NONE -i NONE -N -n -es`・`:normal!`）。
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

using Random = std::mt19937_64;
// 名前・本文・鍵・Vim 9.1 の本文。
using MeasuredRow = std::tuple<const char *, std::string_view, std::string_view, std::string_view>;
// 本文・行・桁・Vim のモード。
using Outcome = std::tuple<std::string, std::size_t, std::size_t, core::VimMode>;

// Vim 9.1 の INSERT の挿入点は結合文字の手前（Vim の 1 文字の途中）に残る。窓の道でも同じ本文に
// なることを見る（`<Esc>j` は -es で Alt の鍵に読まれるので、次の行へは `+` で行く）。
constexpr std::array<MeasuredRow, 10> measured_rows{{
    {"i before a lone mark", "\u0301a", "iab<Esc>", "ab\u0301a"},
    {"I on tab, space and mark", "\t \u0301x", "Iz<Esc>", "\t z\u0301x"},
    {"I on space and mark", " \u0301x", "Iz<Esc>", " z\u0301x"},
    {"a on a lone mark", "\u0301a", "az<Esc>", "\u0301za"},
    {"A after a marked end", "e\u0301x\u0301", "Az<Esc>", "e\u0301x\u0301z"},
    {"o below a lone mark", "\u0301a", "oz<Esc>", "\u0301a\nz"},
    {"INSERT <Left> before a lone mark", "\u0301a", "ia<Left>b<Esc>", "ba\u0301a"},
    {"INSERT <Right> over a lone mark", "\u0301a", "i<Right>z<Esc>", "\u0301za"},
    {"INSERT <C-r>a before a lone mark", "\u0301a\nxy", "j\"ay$k0i<C-r>az<Esc>", "xyz\u0301a\nxy"},
    {"dot replays the typed insert", "\u0301a\n\u0301a", "iab<Esc>+.", "ab\u0301a\nab\u0301a"},
}};

[[nodiscard]] Outcome outcome_of(EditorController &controller)
{
    const auto frame = controller.frame();
    return Outcome{vim_body(frame), frame.caret.position.line.value,
                   frame.caret.position.column.value, controller.vim_state().mode};
}

// 窓と同じに鍵を 1 つずつ入口の意図で送る。
[[nodiscard]] Outcome typed_in_window(std::string body, std::string_view keys)
{
    Editing editing;
    open_vim_document(editing, std::move(body));
    for (const auto &key : vim_keys_of(keys))
    {
        static_cast<void>(applied(editing.controller(), app::VimKeyPress{key}));
    }
    return outcome_of(editing.controller());
}

// fixture と同じに入口を通らない press_vim_key で流す（失敗しても次の鍵が走る）。
[[nodiscard]] Outcome typed_as_fixture(std::string body, std::string_view keys)
{
    Editing editing;
    open_vim_document(editing, std::move(body));
    vim_replay(editing.controller(), keys);
    return outcome_of(editing.controller());
}

void verify_measured_rows()
{
    for (const auto &[name, body, keys, vim] : measured_rows)
    {
        const std::string typed = std::get<0>(typed_in_window(std::string(body), keys));
        if (typed != vim)
        {
            std::fprintf(stderr, "  window path %s: \"%s\"\n", name, typed.c_str());
        }
        expect(typed == vim, "the window path types the same text as Vim 9.1 around combining "
                             "marks (#1309)");
    }
}

// ---------------------------------------------------------------- 性質（固定 seed）

constexpr std::size_t property_cases = 48U;
// 行頭の孤立した結合文字と空白に付いた結合文字は INSERT の挿入点を文字の途中に置く（Vim と同じ）。
constexpr std::array<std::string_view, 8> body_tokens{"ab",       " ",      "\t",   "e\u0301",
                                                      "か\u3099", "\u0301", "日本", " \u0301"};
// INSERT の打鍵（文字・結合文字・矢印・BS）と、モードの出入り・NORMAL の移動と編集・`.`・`u`。
constexpr std::array<std::string_view, 22> key_words{
    "i", "a", "I", "A", "o", "<Esc>", "x", "\u0301", "<Left>", "<Right>", "<BS>",
    "h", "l", "$", "0", "j", "k",     "X", ".",      "u",      "<Tab>",   "e\u0301"};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::string random_body(Random &random)
{
    const std::size_t lines = 1U + below(random, 3U);
    std::string body;
    for (std::size_t line = 0; line < lines; ++line)
    {
        if (line > 0U)
        {
            body += "\n";
        }
        if (below(random, 2U) == 0U)
        {
            body += "\u0301";
        }
        const std::size_t tokens = below(random, 5U);
        for (std::size_t token = 0; token < tokens; ++token)
        {
            body += body_tokens.at(below(random, body_tokens.size()));
        }
    }
    return body;
}

[[nodiscard]] std::string random_keys(Random &random)
{
    const std::size_t count = 6U + below(random, 15U);
    std::string keys;
    for (std::size_t index = 0; index < count; ++index)
    {
        keys += key_words.at(below(random, key_words.size()));
    }
    return keys;
}

void verify_paths_agree()
{
    Random random{13091U};
    std::size_t differing = 0;
    for (std::size_t index = 0; index < property_cases; ++index)
    {
        const std::string body = random_body(random);
        const std::string keys = random_keys(random);
        const Outcome window = typed_in_window(body, keys);
        if (window != typed_as_fixture(body, keys) && differing++ == 0U)
        {
            std::fprintf(stderr,
                         "  window and fixture paths differ\n    body: \"%s\"\n"
                         "    keys: %s\n    window: \"%s\"\n",
                         body.c_str(), keys.c_str(), std::get<0>(window).c_str());
        }
    }
    expect(differing == 0U, "the window path and the fixture path reach the same text, caret and "
                            "mode (#1309 property)");
}
} // namespace

void verify_vim_window_path()
{
    verify_measured_rows();
    verify_paths_agree();
}
} // namespace nenenib::tests
