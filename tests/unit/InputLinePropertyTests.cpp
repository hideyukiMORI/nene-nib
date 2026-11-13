// scope `--input-line-property` の性質の試験（#1616・ADR 0022 の追補 #1568 / #1616）。入力行の
// 全部の入口（Vim の `:` `/` `?`・通常モードの「検索: 」「置換後: 」「行: 」）に 0〜4,200 バイトの
// 乱数の文（ASCII・全角・`/` `\` `&` を混ぜる・`:` は `:s` の族の頭を付けることがある）を 1 回の
// 差し込みで入れ、Enter / Esc / F3 で終える。どの組でもプロセスが落ちず（例外が抜けない）、
// あふれた行は Enter / F3 / Esc のどれでも本文・キャレット・未保存の印を変えず、確定の口（Enter と
// 「検索: 」「置換後: 」の F3）は行を閉じて断る。Esc はどの長さでも本文を変えない。#1616 は `:s`
// の知らせの文を 256 バイトで作って落ちた（入力行の上限 4096 の後に 残っていた 256
// の上限）。同じ形の穴を入口と長さの組で機械に探させる。 生成器は std::mt19937_64 の固定 seed
// で、分布の型は使わず剰余で範囲に畳む。
#include "BeginFind.hpp"
#include "BeginGoToLine.hpp"
#include "BeginReplace.hpp"
#include "CancelCommand.hpp"
#include "CommandText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "InputText.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SearchHop.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "VimSearchDirection.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using core::input_line_limit;
using Random = std::mt19937_64;

constexpr std::uint64_t seed = 1616;
// 入口 6 つ × 終わり方 3 つ × 周。Debug（ASan）で scope 全体が数十秒に収まる値。
constexpr std::size_t rounds = 34;
constexpr std::size_t longest = 4200;
constexpr std::string_view body = "one\ntwo a/b\n\\three&\n界four\n";

constexpr std::array<std::string_view, 16> pieces{"a", "b",   "c",   "/", "\\", "&", "界", "あ",
                                                  " ", "\\(", "\\)", "*", ".",  "~", "#",  "1"};
// `:` の行の頭（`:s` の族・範囲・ほかの命令・頭なし）。
constexpr std::array<std::string_view, 10> colon_heads{"s/", "%s/",  "'<,'>s/", "s#",   "&&",
                                                       "~",  "s/a/", "g/",      "set ", ""};
// 区切りの間に置く片（区切りの `/` `#` と、区切りを食べる単独の `\` を除く）。
constexpr std::array<std::string_view, 13> field_pieces{"a",   "b",   "c", "&", "界", "あ", " ",
                                                        "\\(", "\\)", "*", ".", "~",  "1"};
// 字面の通る `:s` の族の形（頭・パターンと置換の間・後ろ）。
using SubstituteForm = std::array<std::string_view, 3>;
constexpr std::array<SubstituteForm, 5> substitute_forms{{
    {"s/", "/", "/"},
    {"%s/", "/", "/g"},
    {"s#", "#", "#"},
    {"'<,'>s/", "/", "/"},
    {"s/a/X/ \"", "", ""},
}};

enum class Entry : std::uint8_t
{
    colon,
    slash,
    question,
    find,
    replace,
    go_to_line,
};

enum class Ending : std::uint8_t
{
    enter,
    escape,
    hop,
};

constexpr std::array<Entry, 6> entries{Entry::colon, Entry::slash,   Entry::question,
                                       Entry::find,  Entry::replace, Entry::go_to_line};
constexpr std::array<Ending, 3> endings{Ending::enter, Ending::escape, Ending::hop};

[[nodiscard]] std::size_t below(Random &random, std::size_t limit)
{
    return static_cast<std::size_t>(random() % limit);
}

// 片を継いで length バイト以上にする（最後の片で少し越えることがある）。
template <std::size_t Count>
void filled(Random &random, std::string &text, std::size_t length,
            const std::array<std::string_view, Count> &from)
{
    const std::size_t end = text.size() + length;
    while (text.size() < end)
    {
        text += from.at(below(random, from.size()));
    }
}

// 0〜longest バイトの乱数の文。
[[nodiscard]] std::string random_text(Random &random, std::string_view head)
{
    std::string text(head);
    filled(random, text, below(random, longest + 1), pieces);
    return text;
}

// `:` の行。半分は字面の通る `:s` の族（区切りの間を区切りの無い片で埋める）で、`:s` の評価の
// 奥（知らせの文・覚え・engine）まで届く。残りは頭（colon_heads）＋乱数の文。
[[nodiscard]] std::string colon_text(Random &random)
{
    const std::size_t form = below(random, 2 * substitute_forms.size());
    if (form >= substitute_forms.size())
    {
        return random_text(random, colon_heads.at(below(random, colon_heads.size())));
    }
    const SubstituteForm &shape = substitute_forms.at(form);
    // パターンの長さ（first）と置換の長さ（target - first）で行が target バイトほどになる。
    const std::size_t target = below(random, longest + 1);
    const std::size_t first = below(random, target + 1);
    std::string text(shape.at(0));
    filled(random, text, first, field_pieces);
    text += shape.at(1);
    filled(random, text, target - first, field_pieces);
    text += shape.at(2);
    return text;
}

[[nodiscard]] bool vim_entry(Entry entry) noexcept
{
    return entry == Entry::colon || entry == Entry::slash || entry == Entry::question;
}

// 入口を開く。「置換後: 」は 1 段目の語を確定して 2 段目を開く。
void opened(app::EditorController &controller, Entry entry)
{
    switch (entry)
    {
    case Entry::colon:
        vim_replay(controller, ":");
        return;
    case Entry::slash:
        vim_replay(controller, "/");
        return;
    case Entry::question:
        vim_replay(controller, "?");
        return;
    case Entry::find:
        static_cast<void>(controller.apply_frame(app::BeginFind{}));
        return;
    case Entry::replace:
        static_cast<void>(controller.apply_frame(app::BeginReplace{}));
        static_cast<void>(controller.apply_frame(app::CommandText{"o"}));
        static_cast<void>(controller.apply_frame(app::SubmitCommand{}));
        return;
    case Entry::go_to_line:
        static_cast<void>(controller.apply_frame(app::BeginGoToLine{}));
        return;
    }
}

[[nodiscard]] app::EditorFrame ended(app::EditorController &controller, Ending ending)
{
    switch (ending)
    {
    case Ending::enter:
        return controller.apply_frame(app::SubmitCommand{});
    case Ending::escape:
        return controller.apply_frame(app::CancelCommand{});
    case Ending::hop:
        return controller.apply_frame(app::SearchHop{core::VimSearchDirection::forward});
    }
    std::unreachable();
}

// 確定の口は Enter と、通常モードの「検索: 」「置換後: 」の F3（Vim の `/` `?` の Ctrl-G と、`:` と
// 「行: 」の F3 は確定ではなく、行は開いたまま・ADR 0022 の追補 #1568 の 3）。
[[nodiscard]] bool confirms(Entry entry, Ending ending) noexcept
{
    return ending == Ending::enter ||
           (ending == Ending::hop && (entry == Entry::find || entry == Entry::replace));
}

// 1 組。落ちなければ戻ってくる。あふれた行の Enter / F3 と Esc は本文を変えない。
void run_case(Random &random, Entry entry, Ending ending, std::size_t &overflowed)
{
    Editing editing;
    open_vim_document(editing, std::string(body));
    auto &controller = editing.controller();
    if (!vim_entry(entry))
    {
        static_cast<void>(controller.apply_frame(app::SelectEditMode{core::EditMode::ordinary}));
    }
    opened(controller, entry);
    const std::string text =
        entry == Entry::colon ? colon_text(random) : random_text(random, std::string_view{});
    const auto before = controller.frame();
    expect(before.command_line.has_value(), "the entry opens an input line");
    static_cast<void>(controller.apply_frame(app::CommandText{text}));
    const auto frame = ended(controller, ending);
    const bool over = text.size() > input_line_limit;
    overflowed += over ? 1 : 0;
    // 本文を読む（whole_vim_body は窓を動かす）のは frame を写した後。
    const bool kept = whole_vim_body(controller) == body &&
                      frame.document.save_state == core::SaveState::saved &&
                      frame.caret.position.line == before.caret.position.line &&
                      frame.caret.position.column == before.caret.position.column;
    expect(!(over || ending == Ending::escape) || kept,
           "an overflowed or cancelled line leaves the text, the caret and the saved state");
    expect(!over || !confirms(entry, ending) || !frame.command_line.has_value(),
           "an overflowed line is refused and closed");
}
} // namespace

void verify_input_line_property_scope()
{
    Random random{seed};
    std::size_t cases = 0;
    std::size_t overflowed = 0;
    // 入口が速く回り、終わり方は入口の 1 周ごとに替わる（6 × 3 の組を 1 周で全部通る）。
    for (std::size_t index = 0; index < rounds * entries.size() * endings.size(); ++index)
    {
        const Entry entry = entries.at(index % entries.size());
        const Ending ending = endings.at((index / entries.size()) % endings.size());
        run_case(random, entry, ending, overflowed);
        ++cases;
    }
    // 長さは 0〜4,200 の一様なので、約 2.5% があふれる。0 件なら生成器が上限の外を作れていない。
    expect(cases == rounds * entries.size() * endings.size() && overflowed > 0,
           "the generator reaches both sides of the input line limit");
}
} // namespace nenenib::tests
