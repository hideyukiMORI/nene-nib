// scope `--saved-state-property` の単体テスト（#1535・ADR 0181 の決定 4・ADR 0010 の決定 7 と追補
// #1049・ADR 0133 の決定 2）。未保存の判定は本文の位置（EditHistory::content_position）と保存時点の
// 一致で決まるので、判定と同じ式の模型では同語反復になる（V-1530 の
// L2）。ここでは判定の式を使わず、 保存の port が実際に受けたバイト列と比べる。 固定 seed
// の決定的な本文 3 つ（LF か CRLF・BOM つきもある・数字だけの行と空の行を含む）を開き、 Vim
// の鍵（本文を変える鍵・saved_only を作る数字の上の `~` `g~~` `guu`・行頭の `X`・空の行の `x`・
// 範囲が空の changed を作る空の行の `>>` `cc<Esc>`・`u` `<C-r>` `.`・マクロ・`:w`・`:set ff=`
// `:set fenc=`・保存時点より前へ戻ってから別の編集）・通常モードの編集と Ctrl+Z /
// Ctrl+Y・上書き保存
// （Ctrl+S）・名前を付けて保存・保存の形の選択・タブの切り替え・モードの切り替えを混ぜた列を
// EditorController に流し、毎歩の後に次を見る。
//   1. アクティブなタブが保存済みなら、そのファイルへ保存の port が最後に書いたバイト列（開いた
//      だけなら読んだバイト列）＝ 今の本文を今の保存の形（文字コードと改行の形）で写したバイト列。
//      逆向き（本文が同じなら保存済み）は求めない（Vim が C と数える本文の同じ編集がある）。
//   2. 帯の未保存の印と documents() の値が一致し、タブを閉じるときに聞くか（tab_unsaved）と窓を
//      閉じるときに聞くか（exit_plan の未保存の数）が印と一致する。
// port は替え玉だけ（ファイル・クリップボード・時計に触れない）。写しの模型（BOM と改行の写し）は
// 製品の encoded_parts を使わずに書く（改行の写しは core の line_endings_converted の 1 本）。
// 鍵と意図は窓と同じにモードで振り分ける（Vim の鍵は Vim のモードのときだけ・通常の編集と
// Ctrl+Z / Ctrl+Y は通常モードのときだけ・保存と形とタブは両方）。
#include "CaretMotion.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DeleteUnit.hpp"
#include "DocumentView.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "ExitCause.hpp"
#include "ExitPlan.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineEnding.hpp"
#include "LineEndingConversion.hpp"
#include "MoveCaret.hpp"
#include "NewLine.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SelectAll.hpp"
#include "SelectDocumentFormat.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextViewGeometry.hpp"
#include "UnsavedTab.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iterator>
#include <map>
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
// Vim の鍵の記法の列（Vim のモードのときだけ流す）。
using VimKeys = std::string;
// 名前を付けて保存（新しいパスへ、この文字コードで）。
using SaveAs = core::TextEncoding;
// 上書き保存（Ctrl+S・今のパスと文字コード）。
using SaveCurrent = std::monostate;
using Action = std::variant<app::EditorIntent, VimKeys, SaveAs, SaveCurrent>;
// 1 歩: 報告に出す名前と動作。
using Step = std::pair<std::string, Action>;
using Bodies = std::array<std::string, 3>;
using Case = std::pair<Bodies, std::vector<Step>>;
using Reason = std::string_view;
using Check = Reason (*)(const Case &);
// パス → 保存の port がそのパスへ最後に書いたバイト列（開いただけなら読んだバイト列）。
using Written = std::map<std::string, std::string, std::less<>>;
// 届いた形の数（不変条件が見ている状態に列が届くか）。
using Reached = std::array<std::size_t, 11>;
// 届いた形の欄の数。
constexpr std::size_t reach_kinds = 11U;

// Debug（ASan）で 10 秒前後の回数。20 倍の走りは rounds を 20 にして手元で回し、commit しない。
constexpr std::size_t rounds = 1U;
constexpr std::size_t family_cases = 6U * rounds;
constexpr std::size_t reach_cases = 8U;
constexpr std::size_t step_floor = 40U;
constexpr std::size_t step_spread = 41U;
constexpr std::size_t visible_lines = 40U;
constexpr std::string_view byte_order_mark = "\xEF\xBB\xBF";
constexpr std::string_view saved_as_prefix = "C:\\s\\saved-";

constexpr std::array<std::string_view, 3> paths{"C:\\s\\a.txt", "C:\\s\\b.txt", "C:\\s\\c.txt"};
// 本文の行。数字だけの行（`~` `g~~` `guu` が saved_only）と空の行（`x` が saved_only・`>>` `cc` が
// 範囲の空の changed）を厚くする。
constexpr std::array<std::string_view, 10> body_lines{
    "", "", "123", "42", "abc", "Hello World", "  ab", "日本語", "\tx", "a1b2"};
// Vim の鍵の列。生成器が先頭に `<Esc>` を付ける（ノーマルへ戻してから打つ）。
constexpr std::array<std::string_view, 66> vim_fragments{
    // 本文を変える鍵
    "x", "dd", "p", "P", "ix<Esc>", "Ayz<Esc>", "o<Esc>", "O1<Esc>", "J", "rZ", "~", "dw", "yyp",
    "cwq<Esc>", "<C-a>", "D", "i<CR><Esc>", "ix<BS><Esc>",
    // saved_only を作る鍵（数字の上・行頭・空の行・`"_p`・数の無い <C-x>）
    "g~~", "guu", "gUU", "0X", "0~", "0x", "\"_p", "<C-x>", "0g~w",
    // 範囲が空の changed を作る鍵（空の行で）
    ">>", "<<", "cc<Esc>", "C<Esc>",
    // 移動
    "j", "k", "gg", "G", "0", "$", "w", "jj",
    // undo / redo / `.`
    "u", "<C-r>", "uu", ".", "3u", "u<C-r>", "2<C-r>",
    // 保存時点より前へ戻ってから別の編集（枝を捨てる）
    "uuix<Esc>", "uu~", "uux", "3uix<Esc>",
    // マクロ
    "qa~jq", "qaxq", "qbg~~jq", "qb>>jq", "@a", "@b", "@@", "2@a", "qa~jq@a", "qbxq2@b",
    // 保存と保存の形
    ":w<CR>", ":w<CR>", ":set ff=dos<CR>", ":set ff=unix<CR>", ":set fenc=utf-8<CR>",
    ":set fenc=utf-8-bom<CR>"};
// `<Esc>` を付けずに打つ鍵（INSERT の続き・保存の後に打鍵が続く形）。ノーマルでも害が無い。
constexpr std::array<std::string_view, 4> raw_fragments{"ab", "<BS>", "<CR>", "9"};
// Vim のモードで空の単位を作ってすぐ通常モードの Ctrl+Z を押す組（make_mode_crossing）。`o<Esc>` の
// 空の行で `>>` `cc` は範囲が空の changed、`o1<Esc>` の数字の上の `~` `g~~` は saved_only を作る。
constexpr std::array<std::string_view, 4> crossing_fragments{"o<Esc>>>", "o<Esc>cc<Esc>",
                                                             "o1<Esc>~", "o1<Esc>g~~"};
constexpr std::array<std::string_view, 4> typed_texts{"a", "1", "あ", "\t"};
constexpr std::array<core::CaretMotion, 6> motions{
    core::CaretMotion::previous_line,  core::CaretMotion::next_line,
    core::CaretMotion::line_start,     core::CaretMotion::line_end,
    core::CaretMotion::document_start, core::CaretMotion::document_end};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

template <std::size_t N>
[[nodiscard]] std::string_view pick(Random &random, const std::array<std::string_view, N> &pool)
{
    return pool.at(below(random, N));
}

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    auto parsed = core::FilePath::parse(text);
    return std::move(parsed).value();
}

// ---------------------------------------------------------------- 写しの模型（不変条件 1）

// 本文を保存の形で写したバイト列。製品の encoded_parts を使わない。UTF-8 と UTF-8 BOM のほかは
// 生成しないので、ほかの文字コードは比べられない印を返す。
[[nodiscard]] std::string encoded(const core::TextBuffer &text, core::TextEncoding encoding,
                                  core::LineEnding ending)
{
    std::string body = ending == text.line_ending()
                           ? text.text()
                           : core::line_endings_converted(text.text(), ending);
    switch (encoding)
    {
    case core::TextEncoding::utf8:
        return body;
    case core::TextEncoding::utf8_bom:
        return std::string(byte_order_mark) + body;
    case core::TextEncoding::shift_jis:
    case core::TextEncoding::utf16_le:
    case core::TextEncoding::utf16_be:
        break;
    }
    return "(an encoding the generator does not make)";
}

// ステータスバーの改行の欄（保存の形・ADR 0010 の追補 #1049 の決定 1）。
[[nodiscard]] core::LineEnding shown_ending(const app::EditorFrame &frame)
{
    return frame.status_items.at(2).text() == core::line_ending_label(core::LineEnding::crlf)
               ? core::LineEnding::crlf
               : core::LineEnding::lf;
}

// ---------------------------------------------------------------- 不変条件

[[nodiscard]] Reason mark_violation(const app::EditorFrame &frame,
                                    const std::vector<app::DocumentView> &views)
{
    if (views.size() != frame.tabs.size() || frame.active_tab >= views.size())
    {
        return "2: documents() and the band disagree on the tabs";
    }
    std::size_t marked = 0;
    for (std::size_t index = 0; index < views.size(); ++index)
    {
        const bool modified = frame.tabs.at(index).save_state != core::SaveState::saved;
        if (frame.tabs.at(index).save_state != views.at(index).save_state)
        {
            return "2: the band mark and documents() disagree";
        }
        if (app::tab_unsaved(views, index) != modified)
        {
            return "2: closing a tab asks differently from its mark";
        }
        marked += modified ? 1U : 0U;
    }
    const bool asks = app::exit_plan(app::ExitCause::user_close, app::unsaved_tab_count(views)) !=
                      app::ExitPlan::leave_now;
    return asks == (marked > 0U) ? Reason{}
                                 : "2: closing the window asks differently from the marks";
}

[[nodiscard]] Reason bytes_violation(app::EditorController &controller, const Written &written,
                                     const app::EditorFrame &frame,
                                     const std::vector<app::DocumentView> &views)
{
    const auto &view = views.at(frame.active_tab);
    if (view.save_state != core::SaveState::saved)
    {
        return {};
    }
    if (!view.path.has_value())
    {
        return "1: a saved tab has no file";
    }
    const auto found = written.find(view.path.value().text());
    if (found == written.end())
    {
        return "1: a saved tab's file was never read or written";
    }
    return encoded(controller.text(), view.encoding, shown_ending(frame)) == found->second
               ? Reason{}
               : "1: a saved tab's text in its save form differs from the bytes last saved";
}

[[nodiscard]] Reason violation(app::EditorController &controller, const Written &written,
                               app::EditorFrame &frame)
{
    frame = controller.frame();
    const auto views = controller.documents();
    const Reason reason = mark_violation(frame, views);
    return reason.empty() ? bytes_violation(controller, written, frame, views) : reason;
}

// ---------------------------------------------------------------- 走り

// 窓が通常モードでだけ送る意図（Vim のモードでは鍵が engine へ行く・Ctrl+Z / Ctrl+Y は Vim
// に譲る）。
[[nodiscard]] bool ordinary_only(const app::EditorIntent &intent)
{
    return std::holds_alternative<app::InsertText>(intent) ||
           std::holds_alternative<app::NewLine>(intent) ||
           std::holds_alternative<app::DeleteText>(intent) ||
           std::holds_alternative<app::MoveCaret>(intent) ||
           std::holds_alternative<app::SelectAll>(intent) ||
           std::holds_alternative<app::HistoryAction>(intent) ||
           std::holds_alternative<app::ClipboardAction>(intent);
}

void open_files(Editing &editing, const Bodies &bodies, Written &written)
{
    for (std::size_t index = 0; index < paths.size(); ++index)
    {
        editing.files().hold_at(std::string(paths.at(index)), bodies.at(index));
        written.insert_or_assign(std::string(paths.at(index)), bodies.at(index));
    }
    static_cast<void>(editing.controller().apply(app::VisibleLines{
        visible_lines, core::TextViewGeometry{400, 20, 96, 10.0F}, 800, 600, 20}));
    for (const auto path : paths)
    {
        static_cast<void>(editing.controller().apply(app::OpenDocument{path_of(path)}));
    }
}

void save(app::EditorController &controller, std::string_view path, core::TextEncoding encoding)
{
    static_cast<void>(controller.apply(app::SaveDocument{path_of(path), encoding}));
}

// 上書き保存（Ctrl+S）: 帯の位置 tab のタブの今のパスと文字コードで保存する。
void save_current(app::EditorController &controller, std::size_t tab)
{
    const auto views = controller.documents();
    const auto &view = views.at(tab);
    if (view.path.has_value())
    {
        save(controller, view.path.value().text(), view.encoding);
    }
}

// 窓と同じにモードで振り分けて流す（Vim の鍵は Vim のモードだけ・通常の編集は通常モードだけ）。
void routed(app::EditorController &controller, const Action &action, bool vim)
{
    if (const auto *keys = std::get_if<VimKeys>(&action); keys != nullptr && vim)
    {
        vim_replay(controller, *keys);
    }
    const auto *intent = std::get_if<app::EditorIntent>(&action);
    if (intent != nullptr && (!vim || !ordinary_only(*intent)))
    {
        static_cast<void>(controller.apply(*intent));
    }
}

// 1 歩を当てる。frame は直前の frame（モードと帯の位置を読む）。名前を付けて保存のパスは覚えた
// ファイルの数で決める（名前を付けて保存だけが新しいパスを足すので重ならない）。
void apply_step(Editing &editing, const Action &action, const app::EditorFrame &frame,
                Written &written)
{
    auto &controller = editing.controller();
    routed(controller, action, frame.mode == core::EditMode::vim);
    if (const auto *encoding = std::get_if<SaveAs>(&action))
    {
        save(controller, std::string(saved_as_prefix) + std::to_string(written.size()) + ".txt",
             *encoding);
    }
    if (std::holds_alternative<SaveCurrent>(action))
    {
        save_current(controller, frame.active_tab);
    }
    if (!editing.files().written_path().empty())
    {
        written.insert_or_assign(editing.files().written_path(), editing.files().written());
    }
}

[[nodiscard]] bool active_saved(const app::EditorController &controller, std::size_t tab)
{
    return controller.documents().at(tab).save_state == core::SaveState::saved;
}

[[nodiscard]] bool saving_step(const Step &step)
{
    return std::holds_alternative<SaveAs>(step.second) ||
           std::holds_alternative<SaveCurrent>(step.second) ||
           step.first.find(":w") != std::string::npos;
}

[[nodiscard]] bool switching_step(const Step &step)
{
    const auto *intent = std::get_if<app::EditorIntent>(&step.second);
    return intent != nullptr && std::holds_alternative<app::SwitchTab>(*intent);
}

// 通常モードの Ctrl+Z / Ctrl+Y の歩（frame は歩の後の frame・履歴の歩はモードを変えない）。
[[nodiscard]] bool ordinary_history_step(const Step &step, const app::EditorFrame &frame,
                                         core::HistoryDirection direction)
{
    const auto *intent = std::get_if<app::EditorIntent>(&step.second);
    const auto *history = intent == nullptr ? nullptr : std::get_if<app::HistoryAction>(intent);
    return history != nullptr && history->direction == direction &&
           frame.mode == core::EditMode::ordinary;
}

[[nodiscard]] std::size_t distance(std::size_t left, std::size_t right)
{
    return left > right ? left - right : right - left;
}

// 歩の前の「アクティブなタブが保存済みか」と履歴と本文。
using Before = std::tuple<bool, core::EditHistory, std::string>;

// この歩で届いた形（verify_generator_reaches_the_states）。frame は歩の後の frame。
[[nodiscard]] std::array<bool, reach_kinds>
reached_now(const Step &step, const Before &prior, Editing &editing, const app::EditorFrame &frame)
{
    const bool was_saved = std::get<0>(prior);
    const core::EditHistory &before = std::get<1>(prior);
    const core::EditHistory &after = editing.controller().history();
    const bool saved = active_saved(editing.controller(), frame.active_tab);
    const bool saving = saving_step(step);
    const std::string &bytes = editing.files().written();
    const bool switched = switching_step(step);
    const bool undo = ordinary_history_step(step, frame, core::HistoryDirection::undo);
    const bool history = undo || ordinary_history_step(step, frame, core::HistoryDirection::redo);
    return std::array<bool, reach_kinds>{
        // saved_only の単位を挟んで保存済みのまま
        saved && after.position() > after.content_position(),
        // 本文と違う改行の形で保存した
        saving && shown_ending(frame) != editing.controller().text().line_ending(),
        // BOM つきで保存した
        saving && bytes.starts_with(byte_order_mark),
        // 保存しない歩で未保存から保存済みへ戻った（保存時点への undo・形を戻す）
        !was_saved && saved && !saving,
        // 名前を付けて保存した
        editing.files().written_path().starts_with(saved_as_prefix),
        // 履歴を持つタブへ切り替えた
        switched && after.size() > 0U,
        // マクロを再生して履歴が動いた
        step.first.find('@') != std::string::npos && after.revision() != before.revision(),
        // redo の枝を捨てて積んだ（件数が減るのは 2 件以上を捨てて 1 件を積んだときだけ）
        after.size() < before.size(),
        // 通常モードの Ctrl+Z / Ctrl+Y が saved_only の空の単位を飛ばした（1 回で 2 件以上動いた）
        history && distance(after.position(), before.position()) >= 2U,
        // 通常モードの Ctrl+Z が changed の空の単位を消費した（本文は同じで本文の位置が 1 戻った）
        undo && editing.controller().text().text() == std::get<2>(prior) &&
            after.content_position() + 1U == before.content_position(),
        // 保存しない歩で文字コードだけを変えて未保存になった
        step.first.starts_with("format utf") && was_saved && !saved};
}

void count_reached(Reached &reached, const std::array<bool, reach_kinds> &now)
{
    for (std::size_t flag = 0; flag < now.size(); ++flag)
    {
        reached.at(flag) += now.at(flag) ? 1U : 0U;
    }
}

// 列を流し、開いた後と毎歩の後に不変条件を見る。最初に破れた不変条件を返す（空なら無し）。
// reached が空でなければ届いた形を数える（そのときは破れても最後まで流す）。
[[nodiscard]] Reason walked(const Case &input, Reached *reached)
{
    Editing editing;
    Written written;
    open_files(editing, input.first, written);
    auto &controller = editing.controller();
    app::EditorFrame frame = controller.frame();
    Reason reason = violation(controller, written, frame);
    for (const auto &step : input.second)
    {
        if (!reason.empty() && reached == nullptr)
        {
            return reason;
        }
        const Before prior{active_saved(controller, frame.active_tab), controller.history(),
                           controller.text().text()};
        apply_step(editing, step.second, frame, written);
        const Reason now = violation(controller, written, frame);
        reason = reason.empty() ? now : reason;
        if (reached != nullptr)
        {
            count_reached(*reached, reached_now(step, prior, editing, frame));
        }
    }
    return reason;
}

[[nodiscard]] Reason case_violation(const Case &input)
{
    return walked(input, nullptr);
}

// ---------------------------------------------------------------- 生成器

[[nodiscard]] Step intent_step(std::string label, app::EditorIntent intent)
{
    return Step{std::move(label), Action{std::move(intent)}};
}

[[nodiscard]] Step keys_step(std::string keys)
{
    std::string label = "vim " + keys;
    return Step{std::move(label), Action{VimKeys{std::move(keys)}}};
}

using Steps = std::vector<Step>;

void make_vim(Random &random, Steps &steps)
{
    if (below(random, 6U) == 0U)
    {
        steps.push_back(keys_step(std::string(pick(random, raw_fragments))));
        return;
    }
    steps.push_back(keys_step("<Esc>" + std::string(pick(random, vim_fragments))));
}

void make_ordinary(Random &random, Steps &steps)
{
    const std::size_t kind = below(random, 10U);
    const std::size_t motion = below(random, motions.size());
    const std::string_view text = pick(random, typed_texts);
    const bool cut = below(random, 2U) == 0U;
    const std::array<Step, 10> made{
        intent_step("insert " + std::string(text), app::InsertText{std::string(text)}),
        intent_step("new_line", app::NewLine{}),
        intent_step("backspace",
                    app::DeleteText{core::DeleteDirection::backward, core::DeleteUnit::character}),
        intent_step("delete",
                    app::DeleteText{core::DeleteDirection::forward, core::DeleteUnit::character}),
        intent_step("move " + std::to_string(motion),
                    app::MoveCaret{motions.at(motion), core::SelectionAnchoring::collapse}),
        intent_step("select_all", app::SelectAll{}),
        intent_step("ctrl+z", app::HistoryAction{core::HistoryDirection::undo}),
        intent_step("ctrl+z", app::HistoryAction{core::HistoryDirection::undo}),
        intent_step("ctrl+y", app::HistoryAction{core::HistoryDirection::redo}),
        cut ? intent_step("cut", app::ClipboardAction{app::ClipboardOperation::cut})
            : intent_step("paste", app::ClipboardAction{app::ClipboardOperation::paste})};
    steps.push_back(made.at(kind));
}

void make_save(Random &random, Steps &steps)
{
    const std::size_t kind = below(random, 4U);
    if (kind < 2U)
    {
        steps.emplace_back("ctrl+s", Action{SaveCurrent{}});
        return;
    }
    const bool bom = kind == 3U;
    steps.emplace_back(
        bom ? "save_as utf-8-bom" : "save_as utf-8",
        Action{SaveAs{bom ? core::TextEncoding::utf8_bom : core::TextEncoding::utf8}});
}

void make_format(Random &random, Steps &steps)
{
    const std::size_t kind = below(random, 4U);
    const std::array<Step, 4> made{
        intent_step("format lf", app::SelectDocumentFormat{core::LineEnding::lf}),
        intent_step("format crlf", app::SelectDocumentFormat{core::LineEnding::crlf}),
        intent_step("format utf-8", app::SelectDocumentFormat{core::TextEncoding::utf8}),
        intent_step("format utf-8-bom", app::SelectDocumentFormat{core::TextEncoding::utf8_bom})};
    steps.push_back(made.at(kind));
}

void make_tab(Random &random, Steps &steps)
{
    const std::size_t position = below(random, paths.size());
    steps.push_back(intent_step("switch " + std::to_string(position), app::SwitchTab{position}));
}

void make_mode(Random &random, Steps &steps)
{
    const bool vim = below(random, 3U) != 0U;
    steps.push_back(
        intent_step(vim ? "mode vim" : "mode ordinary",
                    app::SelectEditMode{vim ? core::EditMode::vim : core::EditMode::ordinary}));
}

// Vim のモードで空の単位を作り、Vim の編集を挟まずに通常モードへ切り替えて Ctrl+Z を押す
// （V-1541 の M1: 通常モードの Ctrl+Z が空の単位に当たる形に届かせる）。
void make_mode_crossing(Random &random, Steps &steps)
{
    steps.push_back(intent_step("mode vim", app::SelectEditMode{core::EditMode::vim}));
    steps.push_back(keys_step("<Esc>" + std::string(pick(random, crossing_fragments))));
    steps.push_back(intent_step("mode ordinary", app::SelectEditMode{core::EditMode::ordinary}));
    steps.push_back(intent_step("ctrl+z", app::HistoryAction{core::HistoryDirection::undo}));
}

using Maker = void (*)(Random &, Steps &);
// 行の重みは並べた回数。Vim の鍵を厚くし、通常の編集・保存・形・タブ・モードを混ぜる。
constexpr std::array<Maker, 15> makers{make_vim,      make_vim,  make_vim,          make_vim,
                                       make_vim,      make_vim,  make_ordinary,     make_ordinary,
                                       make_ordinary, make_save, make_save,         make_format,
                                       make_tab,      make_mode, make_mode_crossing};

[[nodiscard]] std::string random_body(Random &random)
{
    const std::size_t lines = 1U + below(random, 5U);
    const std::string_view newline = below(random, 2U) == 0U ? "\n" : "\r\n";
    std::string body = below(random, 5U) == 0U ? std::string(byte_order_mark) : std::string();
    for (std::size_t line = 0; line < lines; ++line)
    {
        body += line == 0U ? std::string_view{} : newline;
        body += pick(random, body_lines);
    }
    return body;
}

[[nodiscard]] Step mode_vim()
{
    return intent_step("mode vim", app::SelectEditMode{core::EditMode::vim});
}

[[nodiscard]] Case random_case(Random &random)
{
    Case input;
    for (auto &body : input.first)
    {
        body = random_body(random);
    }
    input.second.push_back(mode_vim());
    const std::size_t target = step_floor + below(random, step_spread);
    while (input.second.size() < target)
    {
        makers.at(below(random, makers.size()))(random, input.second);
    }
    return input;
}

// ---------------------------------------------------------------- 縮めて報告する

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
    Random left{15350U};
    Random right{15350U};
    bool same = true;
    for (std::size_t round = 0; round < 3U; ++round)
    {
        same = same && labels_of(random_case(left)) == labels_of(random_case(right));
    }
    expect(same, "the saved-state generator gives the same cases for the same seed");
}

// 生成した列が、saved_only の単位を挟んだ保存済み・本文と違う改行の形の保存・BOM つきの保存・
// 保存しない歩での保存済みへの戻り・名前を付けて保存・履歴を持つタブへの切り替え・マクロの再生・
// redo の枝を捨てる編集・通常モードの Ctrl+Z / Ctrl+Y が saved_only の空の単位を飛ばす歩・通常
// モードの Ctrl+Z が changed の空の単位を消費する歩・文字コードだけを変えて未保存になる歩に届く。
// どれかが 0 なら性質の試験はその形を見ていない。
void verify_generator_reaches_the_states()
{
    Random random{15351U};
    Reached reached{};
    for (std::size_t index = 0; index < reach_cases; ++index)
    {
        static_cast<void>(walked(random_case(random), &reached));
    }
    expect(std::ranges::all_of(reached, [](std::size_t count) { return count > 0U; }),
           "the saved-state cases reach a saved document past a saved_only unit, a converted and a "
           "BOM save, an undo back to the save point, a save as, a tab switch, a macro, a "
           "discarded redo branch, an ordinary undo or redo past a saved_only unit, an ordinary "
           "undo of an empty changed unit and an encoding-only change");
}

[[nodiscard]] Case known_case(std::string body, std::vector<Step> steps)
{
    return Case{Bodies{std::move(body), "", ""}, std::move(steps)};
}

[[nodiscard]] Step ctrl_s()
{
    return Step{"ctrl+s", Action{SaveCurrent{}}};
}

[[nodiscard]] Step mode_ordinary()
{
    return intent_step("mode ordinary", app::SelectEditMode{core::EditMode::ordinary});
}

[[nodiscard]] Step ctrl_z()
{
    return intent_step("ctrl+z", app::HistoryAction{core::HistoryDirection::undo});
}

[[nodiscard]] Step ctrl_y()
{
    return intent_step("ctrl+y", app::HistoryAction{core::HistoryDirection::redo});
}

[[nodiscard]] Step switch_to(std::size_t position)
{
    return intent_step("switch " + std::to_string(position), app::SwitchTab{position});
}

// 手書きの組は本文を帯の 3 つ目（開いた最後のタブ・アクティブ）に置く。
[[nodiscard]] Case flipped(const Case &input)
{
    return Case{Bodies{input.first.at(1), input.first.at(2), input.first.at(0)}, input.second};
}

// V-1530 の L2 の手書きの形（保存 → saved_only を挟む undo → 別の編集）と、INSERT の途中の保存の
// 後に打鍵が続く形・形だけを変えて保存して戻す形を、同じ不変条件で見る。開いた最後のタブ
// （帯の 3 つ目）が本文を持つ。
void verify_known_cases()
{
    const std::array<Case, 6> cases{
        known_case("abc\n123", {mode_vim(), keys_step("x"), keys_step(":w<CR>"), keys_step("j~"),
                                keys_step("u"), keys_step("u"), keys_step("ix<Esc>")}),
        known_case("abc\n123", {mode_vim(), keys_step("x:w<CR>"), keys_step("j~"), keys_step("u"),
                                keys_step("ky")}),
        known_case("abc",
                   {mode_vim(), keys_step("ix"), ctrl_s(), keys_step("yz"), keys_step("<Esc>u")}),
        known_case("a\nb",
                   {intent_step("format crlf", app::SelectDocumentFormat{core::LineEnding::crlf}),
                    ctrl_s(),
                    intent_step("format lf", app::SelectDocumentFormat{core::LineEnding::lf})}),
        known_case("\n123", {mode_vim(), keys_step(">>"), keys_step(":w<CR>"), keys_step("u"),
                             keys_step("j~"), keys_step("<C-r>")}),
        // V-1541 の M1: 保存の後の空の行の `>>` を通常モードの Ctrl+Z が消費する（2 回目で `x`）
        known_case("abc\n\n", {mode_vim(), keys_step("x"), keys_step(":w<CR>"), keys_step("j>>"),
                               mode_ordinary(), ctrl_z(), ctrl_z(), ctrl_y(), ctrl_y()})};
    std::size_t broken = 0;
    for (const auto &input : cases)
    {
        broken += case_violation(flipped(input)).empty() ? 0U : 1U;
    }
    expect(broken == 0U, "the hand-written save, undo and branch cases keep the saved bytes");
}

// 列を流し終えたときのアクティブなタブの本文と、保存済みか。
using Ending = std::pair<std::string, bool>;

[[nodiscard]] Ending ending_of(const Case &input)
{
    Editing editing;
    Written written;
    open_files(editing, input.first, written);
    auto &controller = editing.controller();
    for (const auto &step : input.second)
    {
        apply_step(editing, step.second, controller.frame(), written);
    }
    return Ending{controller.text().text(),
                  active_saved(controller, controller.frame().active_tab)};
}

// 終わりの本文と印を名指しで期待する手書きの組。不変条件 1 は「保存済みと出る」側の誤りしか
// 見ないので、「未保存と出る」側の誤り（タブを切り替えた後の束の保存時点のずれ・V-1541 の F1）と
// 通常モードの Ctrl+Z が changed の空の単位を消費する規則（M1）をここで見る。
void verify_known_endings()
{
    using Expected = std::pair<Case, Ending>;
    const std::array<Expected, 5> cases{
        // M1: 1 回目の Ctrl+Z は `>>` だけを戻して保存済み・2 回目は `x` を戻して未保存
        Expected{known_case("abc\n\n", {mode_vim(), keys_step("x"), keys_step(":w<CR>"),
                                        keys_step("j>>"), mode_ordinary(), ctrl_z()}),
                 Ending{"bc\n\n", true}},
        Expected{known_case("abc\n\n", {mode_vim(), keys_step("x"), keys_step(":w<CR>"),
                                        keys_step("j>>"), mode_ordinary(), ctrl_z(), ctrl_z()}),
                 Ending{"abc\n\n", false}},
        // F1: 保存 → 数字の上の `~` → ほかのタブへ行って戻る → 保存済み
        Expected{known_case("123", {mode_vim(), keys_step("x"), keys_step(":w<CR>"), keys_step("~"),
                                    switch_to(0), switch_to(2)}),
                 Ending{"23", true}},
        // F1: 保存 → 通常モードの Ctrl+Z → Ctrl+Y → ほかのタブへ行って戻る → 保存済み
        Expected{known_case("abc", {mode_ordinary(), intent_step("insert x", app::InsertText{"x"}),
                                    ctrl_s(), ctrl_z(), ctrl_y(), switch_to(1), switch_to(2)}),
                 Ending{"xabc", true}},
        // F1: 保存 → 空の行の `>>` → `u` → ほかのタブへ行って戻る → 保存済み
        Expected{
            known_case("abc\n\n", {mode_vim(), keys_step("x"), keys_step(":w<CR>"),
                                   keys_step("j>>"), keys_step("u"), switch_to(0), switch_to(2)}),
            Ending{"bc\n\n", true}}};
    std::size_t broken = 0;
    for (const auto &[input, ending] : cases)
    {
        const Case placed = flipped(input);
        const bool kept = case_violation(placed).empty() && ending_of(placed) == ending;
        if (!kept)
        {
            std::fprintf(
                stderr, "  ending: %s -> \"%s\" saved=%d\n", escaped(placed.first.at(2)).c_str(),
                escaped(ending_of(placed).first).c_str(), ending_of(placed).second ? 1 : 0);
        }
        broken += kept ? 0U : 1U;
    }
    expect(broken == 0U, "the hand-written cases end with the expected text and saved mark");
}

// 検査そのものが破れを見つける: 保存済みのタブの覚えたバイト列を書き換えると不変条件 1 が落ちる。
void verify_checks_catch_a_break()
{
    Editing editing;
    Written written;
    open_files(editing, Bodies{"", "", "abc"}, written);
    app::EditorFrame frame = editing.controller().frame();
    const bool kept = violation(editing.controller(), written, frame).empty();
    written.insert_or_assign(std::string(paths.at(frame.active_tab)), "abd");
    const bool caught = !violation(editing.controller(), written, frame).empty();
    expect(kept && caught && frame.active_tab == 2U,
           "the saved-bytes check holds on an opened file and catches other bytes");
}
} // namespace

void verify_saved_state_property_scope()
{
    verify_determinism();
    verify_checks_catch_a_break();
    verify_known_cases();
    verify_known_endings();
    verify_generator_reaches_the_states();
    expect(broken_cases("saved means the saved bytes", 15352U, case_violation) == 0U,
           "Vim keys, ordinary edits, undo and redo, saves, save forms and tab switches keep a "
           "saved tab's text equal to the bytes last saved and the marks equal to the close "
           "questions");
}
} // namespace nenenib::tests
