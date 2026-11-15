// scope `--autosave-restore-property` の性質の試験（#1290・#1688・ADR 0137 の決定 6 と追補・ADR
// 0042）。 前半（#1688）は退避 → 復元の往復。固定 seed の決定的な生成器で作った文書（UTF-8 / UTF-8
// BOM / UTF-16 LE / BE / Shift_JIS（替え玉の CP932・CP932
// で表せない字を含む・書きの変換は失敗を返す）・ LF / CRLF / 改行なし）を開き、Vim
// の鍵で変え（改行なしまで削る形を含む）、保存の形（文字コード・
// 改行の形）を選ぶことがあり、合図で退避する。その写しを次の起動の残骸（一覧は形をそのまま運ぶ）
// として別の Editing で戻し、次を確かめる。
//   R1. 未保存の文書は必ず退避される（CP932 で表せない字があっても）。
//   R2. 復元 == 退避の時点の（本文・文字コード・保存の改行の形・本文の改行の形）で、未保存。
//   R3. 戻したタブの退避は、元の退避と同じ本文のバイト列と同じ形（退避 → 復元 → 退避の往復）。
// 後半（#1290）は前の版（beta.13 まで）が書いた形の無い残骸。残骸の列（本文の無い残骸・正しい
// UTF-8 / UTF-8 BOM / UTF-16 LE / BE の本文・でたらめなバイト列・壊れた UTF-8・UTF-32 の BOM・
// 奇数長の UTF-16・孤立したサロゲート）を替え玉の port ScriptedAutosave から渡し、次を確かめる。
//   1. 戻せる残骸だけがタブになる。戻せるかの模型は「同じバイト列を開く道（OpenDocument）で
//      開けるか」で、別の Editing で開いて決める（開くのと同じ判別と復号・決定 6）。
//   2. 戻したタブは残骸の順に帯の末尾へ並び、パスと文字コードは開く道で開いた文書と同じで、未保存。
//   3. 戻せなかった残骸があれば「戻せませんでした: 」の知らせが立ち、無ければ立たない。
//   4. どの残骸も 1 回ずつ読まれ、1 回ずつ forget される（戻せたものも戻せなかったものも）。
//   5. 戻したタブはすぐにこのプロセスの退避として書かれ、その本文は開く道で開いた本文の UTF-8 で、
//      形は開く道の文字コードと本文の改行の形（保存の形は選んでいない）。正しい族の本文なら、その
//      UTF-8 は生成器が手で作った UTF-8 と同じ。どのタブも別の残骸の本文を持たない。
// 後半では Shift_JIS の変換は CodePagePort の向こう（替え玉は空の本文を返す）なので、正しい本文の
// 族に Shift_JIS は入れない。でたらめなバイト列が Shift_JIS と判定されたときは、性質 1・2・4・5 の
// 「開く道と同じ」だけを見る（本物の CP932 は adapters の --document-chunk の性質が見る）。
// 製品の restored_bundle の 1 行を手元で壊して落ちることを確かめた（#1290・commit しない）。
// 回数は Debug（ASan）で scope 全体が 1 秒前後で終わる値。生成器は std::mt19937_64 の剰余だけを
// 使い、分布の型は使わない。時計と環境変数は読まない。
#include "AutosaveForm.hpp"
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"
#include "AutosaveTick.hpp"
#include "CodePageFailure.hpp"
#include "Column.hpp"
#include "DocumentView.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "OpenDocument.hpp"
#include "RestoreAutosave.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedAutosave.hpp"
#include "SelectDocumentFormat.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "Utf16.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <format>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using Random = std::mt19937_64;

constexpr std::size_t rounds = 1U;
constexpr std::size_t restore_cases = 40U * rounds;
constexpr std::size_t remnant_limit = 6U;
constexpr std::size_t piece_limit = 40U;
constexpr std::uint64_t restore_seed = 1290U;
constexpr std::size_t round_trip_cases = 80U * rounds;
constexpr std::uint64_t round_trip_seed = 1688U;
constexpr std::string_view unrestored_prefix = "戻せませんでした: ";

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] char byte_of(std::uint32_t value)
{
    return static_cast<char>(static_cast<unsigned char>(value & 0xFFU));
}

// 本文の材料（コードポイントの列）。改行は LF・CRLF・CR だけを混ぜる。
constexpr std::array<std::u32string_view, 14> pieces{{U"a", U"word ", U"\t", U"\n", U"\r\n", U"\r",
                                                      U"日本", U"かな", U"\U0001F600", U"e\u0301",
                                                      U"\u200B", U"\u0085", U"\u00E9", U"\uFF21"}};

[[nodiscard]] std::u32string scalars(Random &random)
{
    std::u32string text;
    const std::size_t count = below(random, piece_limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        text.append(pieces.at(below(random, pieces.size())));
    }
    return text;
}

// 製品の写しを使わずに手で符号化する（往復の期待値を製品から独立に作る）。
void append_utf8(std::string &out, char32_t code_point)
{
    const auto value = static_cast<std::uint32_t>(code_point);
    if (value < 0x80U)
    {
        out.push_back(byte_of(value));
        return;
    }
    if (value < 0x800U)
    {
        out.push_back(byte_of(0xC0U | (value >> 6U)));
        out.push_back(byte_of(0x80U | (value & 0x3FU)));
        return;
    }
    if (value < 0x10000U)
    {
        out.push_back(byte_of(0xE0U | (value >> 12U)));
        out.push_back(byte_of(0x80U | ((value >> 6U) & 0x3FU)));
        out.push_back(byte_of(0x80U | (value & 0x3FU)));
        return;
    }
    out.push_back(byte_of(0xF0U | (value >> 18U)));
    out.push_back(byte_of(0x80U | ((value >> 12U) & 0x3FU)));
    out.push_back(byte_of(0x80U | ((value >> 6U) & 0x3FU)));
    out.push_back(byte_of(0x80U | (value & 0x3FU)));
}

void append_unit(std::string &out, std::uint32_t unit, bool little)
{
    out.push_back(byte_of(little ? unit : unit >> 8U));
    out.push_back(byte_of(little ? unit >> 8U : unit));
}

[[nodiscard]] std::string utf8_of(const std::u32string &text)
{
    std::string out;
    for (const char32_t code_point : text)
    {
        append_utf8(out, code_point);
    }
    return out;
}

[[nodiscard]] std::string utf16_of(const std::u32string &text, bool little)
{
    std::string out(little ? core::utf16_le_byte_order_mark() : core::utf16_be_byte_order_mark());
    for (const char32_t code_point : text)
    {
        const auto value = static_cast<std::uint32_t>(code_point);
        if (value < 0x10000U)
        {
            append_unit(out, value, little);
            continue;
        }
        const std::uint32_t offset = value - 0x10000U;
        append_unit(out, 0xD800U | (offset >> 10U), little);
        append_unit(out, 0xDC00U | (offset & 0x3FFU), little);
    }
    return out;
}

[[nodiscard]] std::string random_bytes(Random &random)
{
    std::string out;
    const std::size_t count = below(random, 48U);
    for (std::size_t index = 0; index < count; ++index)
    {
        out.push_back(byte_of(static_cast<std::uint32_t>(below(random, 256U))));
    }
    return out;
}

// 残骸の本文 1 つ（無ければ値なし）と、それが正しい族なら生成器が手で作ったその UTF-8 の本文。
using Body = std::pair<std::optional<std::string>, std::optional<std::string>>;
// 前回のプロセスが残した 1 本と、その本文。
using Planted = std::pair<app::AutosaveRemnant, Body>;
// 開く道の模型の答え。開けなければ値なし、開ければ文字コード・本文（UTF-8）・本文の改行の形。
using OpenedForm = std::tuple<core::TextEncoding, std::string, core::LineEnding>;
using Opened = std::optional<OpenedForm>;
// 1 件の残骸の列と、それぞれの開く道の模型の答え。
using Cases = std::pair<std::vector<Planted>, std::vector<Opened>>;
using Make = std::string (*)(Random &);
// 正しい族の作り手は（バイト列・その UTF-8 の本文）を返す。
using CleanMake = std::pair<std::string, std::string> (*)(Random &);

[[nodiscard]] std::pair<std::string, std::string> clean_utf8(Random &random)
{
    std::string text = utf8_of(scalars(random));
    return {text, text};
}

[[nodiscard]] std::pair<std::string, std::string> clean_utf8_bom(Random &random)
{
    std::string text = utf8_of(scalars(random));
    return {std::string(core::byte_order_mark()) + text, text};
}

[[nodiscard]] std::pair<std::string, std::string> clean_utf16_le(Random &random)
{
    const std::u32string text = scalars(random);
    return {utf16_of(text, true), utf8_of(text)};
}

[[nodiscard]] std::pair<std::string, std::string> clean_utf16_be(Random &random)
{
    const std::u32string text = scalars(random);
    return {utf16_of(text, false), utf8_of(text)};
}

[[nodiscard]] std::string broken_utf8(Random &random)
{
    return utf8_of(scalars(random)) + "\xC3" + utf8_of(scalars(random));
}

[[nodiscard]] std::string utf32_marked(Random &random)
{
    return std::string("\xFF\xFE\x00\x00", 4) + random_bytes(random);
}

[[nodiscard]] std::string odd_utf16(Random &random)
{
    return utf16_of(scalars(random), below(random, 2U) == 0U) + "A";
}

[[nodiscard]] std::string lone_surrogate(Random &random)
{
    return utf16_of(scalars(random), true) + std::string("\x00\xD8", 2) + "A" + '\0';
}

// 正しい族（UTF-8・UTF-8 BOM・UTF-16 LE / BE）と正しくない族（でたらめ・壊れた UTF-8・UTF-32 の
// BOM・奇数長の UTF-16・孤立したサロゲート）。
constexpr std::array<CleanMake, 4> clean_makers{
    {clean_utf8, clean_utf8_bom, clean_utf16_le, clean_utf16_be}};
constexpr std::array<Make, 5> broken_makers{
    {random_bytes, broken_utf8, utf32_marked, odd_utf16, lone_surrogate}};

[[nodiscard]] Body body_of(Random &random)
{
    const std::size_t kind = below(random, 1U + clean_makers.size() + broken_makers.size());
    if (kind == 0U)
    {
        return {std::nullopt, std::nullopt};
    }
    if (kind <= clean_makers.size())
    {
        auto [bytes, text] = clean_makers.at(kind - 1U)(random);
        return {std::move(bytes), std::move(text)};
    }
    return {broken_makers.at(kind - 1U - clean_makers.size())(random), std::nullopt};
}

[[nodiscard]] std::vector<Planted> planted_of(Random &random, std::size_t index)
{
    std::vector<Planted> planted;
    const std::size_t count = below(random, remnant_limit + 1U);
    for (std::size_t at = 0; at < count; ++at)
    {
        std::optional<core::FilePath> path;
        if (below(random, 3U) != 0U)
        {
            path = core::FilePath::parse(std::format("C:/work/r{}-{}.txt", index, at)).value();
        }
        auto title = core::tab_title_for(path, core::SaveState::modified,
                                         nenenib::core::DisplayLanguage::japanese);
        const core::TextPosition caret{core::LineNumber{1U + below(random, 6U)},
                                       core::Column{1U + below(random, 9U)}};
        Body body = body_of(random);
        planted.emplace_back(app::AutosaveRemnant{std::format("dead{}/{}", index, at),
                                                  app::AutosaveId{at}, std::move(path),
                                                  std::move(title), caret},
                             std::move(body));
    }
    return planted;
}

// 開く道の模型: 同じバイト列を別の Editing で開き、開けたら文字コードと本文と本文の改行の形を
// 返す。
[[nodiscard]] Opened opened_of(const Body &body)
{
    if (!body.first.has_value())
    {
        return std::nullopt;
    }
    Editing editing;
    applied(editing.controller(), app::VisibleLines{10});
    editing.files().hold(body.first.value());
    const auto frame = editing.controller().apply_frame(app::OpenDocument{sample_path()});
    if (frame.document.path != std::optional{sample_path()})
    {
        return std::nullopt;
    }
    return OpenedForm{frame.document.encoding, editing.controller().text().text(),
                      editing.controller().text().line_ending()};
}

[[nodiscard]] std::string text_of(const std::vector<std::byte> &bytes)
{
    std::string text;
    text.reserve(bytes.size());
    for (const std::byte value : bytes)
    {
        text.push_back(static_cast<char>(value));
    }
    return text;
}

[[nodiscard]] bool forgotten_all(const ScriptedAutosave &autosave,
                                 const std::vector<Planted> &planted)
{
    if (autosave.forgotten().size() != planted.size())
    {
        return false;
    }
    for (std::size_t at = 0; at < planted.size(); ++at)
    {
        if (autosave.forgotten().at(at) != planted.at(at).first.location)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool message_follows(const app::EditorFrame &frame, std::size_t unrestored)
{
    const bool named = frame.command_message.has_value() &&
                       frame.command_message.value().text().starts_with(unrestored_prefix);
    return named == (unrestored > 0U);
}

// 戻したタブの 1 つが、開く道で開いた文書と同じパス・文字コードで未保存か。
[[nodiscard]] bool tab_follows(const app::DocumentView &view, const Planted &planted,
                               const OpenedForm &model)
{
    return view.path == planted.first.path && view.encoding == std::get<0>(model) &&
           view.save_state == core::SaveState::modified;
}

// 戻したタブの退避の本文が開く道で開いた本文の UTF-8 で、形が開く道の文字コードと本文の改行の形か。
// 正しい族の本文なら、生成器が手で作った UTF-8 と同じか（#1688）。
[[nodiscard]] bool snapshot_follows(const app::AutosaveSnapshot &snapshot, const Planted &planted,
                                    const OpenedForm &model)
{
    const std::string bytes = text_of(snapshot.bytes);
    const bool clean_kept =
        !planted.second.second.has_value() || bytes == planted.second.second.value();
    return snapshot.path == planted.first.path && bytes == std::get<1>(model) &&
           snapshot.form.encoding == std::get<0>(model) && !snapshot.form.save.has_value() &&
           snapshot.form.body == std::get<2>(model) && clean_kept;
}

[[nodiscard]] std::vector<Opened> models_of(const std::vector<Planted> &planted)
{
    std::vector<Opened> models;
    models.reserve(planted.size());
    for (const Planted &one : planted)
    {
        models.push_back(opened_of(one.second));
    }
    return models;
}

// 戻したタブを残骸の順に模型と照らし、戻した数を返す（違いがあれば値なし）。
[[nodiscard]] std::optional<std::size_t> restored_in_order(const app::EditorFrame &frame,
                                                           const ScriptedAutosave &autosave,
                                                           std::size_t before, const Cases &cases)
{
    const auto &[planted, models] = cases;
    std::size_t restored = 0;
    for (std::size_t at = 0; at < planted.size(); ++at)
    {
        const Opened &model = models.at(at);
        if (!model.has_value())
        {
            continue;
        }
        const std::size_t tab = before + restored;
        if (tab >= frame.tabs.size() || restored >= autosave.stored().size() ||
            !tab_follows(frame.tabs.at(tab), planted.at(at), model.value()) ||
            !snapshot_follows(autosave.stored().at(restored), planted.at(at), model.value()))
        {
            return std::nullopt;
        }
        ++restored;
    }
    return restored;
}

// 戻した後の帯・知らせ・読み・forget の数。違いの説明を返す（無ければ空）。
[[nodiscard]] std::string_view settled_violation(const app::EditorFrame &frame,
                                                 const ScriptedAutosave &autosave,
                                                 std::pair<std::size_t, std::size_t> counts,
                                                 const std::vector<Planted> &planted)
{
    const auto [before, restored] = counts;
    if (frame.tabs.size() != before + restored || autosave.stored().size() != restored)
    {
        return "only the remnants opening can open become tabs and are stored";
    }
    if (restored > 0U && frame.active_tab != before)
    {
        return "the first restored tab becomes active";
    }
    if (!message_follows(frame, planted.size() - restored))
    {
        return "an unrestored remnant is told and a full restore tells nothing";
    }
    if (autosave.body_reads() != planted.size() || !forgotten_all(autosave, planted))
    {
        return "each remnant is read once and forgotten once in order";
    }
    return {};
}

// 1 件ぶんを流して、最初に見つけた違いの説明を返す（無ければ空）。
[[nodiscard]] std::string_view case_violation(const std::vector<Planted> &planted)
{
    const Cases cases{planted, models_of(planted)};
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    for (const Planted &one : planted)
    {
        editing.autosave().remain(one.first, one.second.first);
    }
    const std::size_t before = controller.frame().tabs.size();
    if (controller.autosave_remnants().size() != planted.size())
    {
        return "the start keeps every remnant the port lists";
    }
    const app::EditorFrame frame = controller.apply_frame(app::RestoreAutosave{});
    const auto restored = restored_in_order(frame, editing.autosave(), before, cases);
    if (!restored.has_value())
    {
        return "each restored tab follows its remnant: path, encoding, unsaved and the UTF-8 "
               "body and form opening gives (a clean body byte for byte)";
    }
    return settled_violation(frame, editing.autosave(), {before, restored.value()}, planted);
}

void verify_generator_is_deterministic()
{
    Random first{restore_seed};
    Random second{restore_seed};
    const auto left = planted_of(first, 0U);
    const auto right = planted_of(second, 0U);
    bool same = left.size() == right.size();
    for (std::size_t at = 0; same && at < left.size(); ++at)
    {
        same = left.at(at).second.first == right.at(at).second.first &&
               left.at(at).first.location == right.at(at).first.location;
    }
    expect(same, "the remnant generator is deterministic for a seed");
}

// 生成器が模型の両側（戻せる・戻せない）と族の全部に届くこと。数えるのは「正しい・本文の無い・
// 開けない」の 3 つ。
void counted(const Planted &one, std::array<std::size_t, 3> &counts)
{
    counts.at(0) += one.second.second.has_value() ? 1U : 0U;
    counts.at(1) += one.second.first.has_value() ? 0U : 1U;
    counts.at(2) += !one.second.second.has_value() && !opened_of(one.second).has_value() ? 1U : 0U;
}

void verify_generator_reaches_both_sides()
{
    Random random{restore_seed};
    std::array<std::size_t, 3> counts{};
    for (std::size_t index = 0; index < 40U; ++index)
    {
        for (const Planted &one : planted_of(random, index))
        {
            counted(one, counts);
        }
    }
    expect(counts.at(0) > 10U && counts.at(1) > 3U && counts.at(2) > 10U,
           "the generator makes clean, missing and unopenable remnants");
}

// 手で置いた 1 件: 正しい 2 本の間に、本文の無い残骸と UTF-32 の残骸。
void verify_known_case()
{
    auto path = core::FilePath::parse("C:/work/known.txt").value();
    auto title = core::tab_title_for(path, core::SaveState::modified,
                                     nenenib::core::DisplayLanguage::japanese);
    const core::TextPosition caret{core::LineNumber{1}, core::Column{1}};
    const std::vector<Planted> planted{
        {{"k/0", app::AutosaveId{0}, path, title, caret},
         {std::string("alpha\r\nbeta"), std::string("alpha\r\nbeta")}},
        {{"k/1", app::AutosaveId{1}, std::nullopt, title, caret}, {std::nullopt, std::nullopt}},
        {{"k/2", app::AutosaveId{2}, std::nullopt, title, caret},
         {std::string("\xFF\xFE\x00\x00x", 5), std::nullopt}},
        {{"k/3", app::AutosaveId{3}, std::nullopt, title, caret},
         {utf16_of(U"日本\n", false), std::string("日本\n")}}};
    const auto broken = case_violation(planted);
    expect(broken.empty(), "two clean remnants come back around a missing and a UTF-32 one");
}

[[nodiscard]] std::size_t broken_cases()
{
    Random random{restore_seed};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < restore_cases; ++index)
    {
        const auto planted = planted_of(random, index);
        const auto violation = case_violation(planted);
        if (!violation.empty())
        {
            ++broken;
            std::fprintf(stderr, "restore case %zu (seed %llu): %.*s\n", index,
                         static_cast<unsigned long long>(restore_seed),
                         static_cast<int>(violation.size()), violation.data());
        }
    }
    return broken;
}

// --- 前半（#1688）: 退避 → 復元の往復 ---

// 開く文書のバイト列と、替え玉の CP932 が返す本文（Shift_JIS の文書のときだけ）。
using Opening = std::pair<std::string, std::optional<std::string>>;
// 退避した時点の値: 本文・文字コード・本文の改行の形・ステータスバーの改行の欄。
using Kept = std::tuple<std::string, core::TextEncoding, core::LineEnding, std::string>;

// Shift_JIS の文書のバイト列（正しい UTF-8 でないので Shift_JIS と判定される）。本文は替え玉の
// CP932 が返す「日本」と生成した本文で、絵文字（CP932 で表せない字）を含み得る。
constexpr std::string_view sjis_bytes = "\x93\xFA\x96\x7B";

[[nodiscard]] Opening opening_of(Random &random)
{
    const std::u32string text = scalars(random);
    const std::size_t kind = below(random, 5U);
    if (kind == 0U)
    {
        return {utf8_of(text), std::nullopt};
    }
    if (kind == 1U)
    {
        return {std::string(core::byte_order_mark()) + utf8_of(text), std::nullopt};
    }
    if (kind == 2U || kind == 3U)
    {
        return {utf16_of(text, kind == 2U), std::nullopt};
    }
    return {std::string(sjis_bytes), "日本" + utf8_of(text)};
}

// 変える鍵（改行なしまで削る `ggdG`・行をつなぐ `J`・行を足す `o` を含む）。
constexpr std::array<std::string_view, 7> edits{
    {"x", "dd", "ggdG", "2x", "J", "oab<Esc>", "A<Tab><Esc>"}};
constexpr std::array<core::TextEncoding, 5> chosen_encodings{
    core::TextEncoding::utf8, core::TextEncoding::utf8_bom, core::TextEncoding::shift_jis,
    core::TextEncoding::utf16_le, core::TextEncoding::utf16_be};

// 文書を開いて変え、保存の形を選ぶことがある。
void edited(Random &random, Editing &editing)
{
    const Opening opening = opening_of(random);
    if (opening.second.has_value())
    {
        editing.code_pages().decode_to(opening.second.value());
    }
    open_vim_document(editing, opening.first);
    const std::size_t count = below(random, 3U);
    for (std::size_t at = 0; at < count; ++at)
    {
        vim_normal(editing.controller(), edits.at(below(random, edits.size())));
    }
    const std::size_t choice = below(random, 4U);
    if (choice == 1U || choice == 3U)
    {
        applied(editing.controller(),
                app::SelectDocumentFormat{below(random, 2U) == 0U ? core::LineEnding::lf
                                                                  : core::LineEnding::crlf});
    }
    if (choice == 2U || choice == 3U)
    {
        applied(editing.controller(),
                app::SelectDocumentFormat{chosen_encodings.at(below(random, 5U))});
    }
    // CP932 の書きは常に失敗を返す（表せない字の形）。退避はこの口を通らない（#1688 の決定 1）。
    editing.code_pages().encode_to(std::unexpected(app::CodePageFailure::unencodable));
}

[[nodiscard]] Kept kept_of(Editing &editing)
{
    const app::EditorFrame frame = editing.controller().frame();
    return Kept{editing.controller().text().text(), frame.document.encoding,
                editing.controller().text().line_ending(),
                std::string(frame.status_items.at(2).text())};
}

[[nodiscard]] bool same_form(const app::AutosaveForm &left, const app::AutosaveForm &right)
{
    return left.encoding == right.encoding && left.save == right.save && left.body == right.body;
}

// 写しを次の起動の残骸として戻し、戻した値と戻したタブの退避を照らす。違いの説明を返す。
[[nodiscard]] std::string_view restored_violation(const app::AutosaveSnapshot &snapshot,
                                                  const Kept &before)
{
    Editing restore;
    applied(restore.controller(), app::VisibleLines{10});
    restore.autosave().remain(app::AutosaveRemnant{"rt/0", snapshot.id, snapshot.path,
                                                   snapshot.title, snapshot.caret, snapshot.form},
                              text_of(snapshot.bytes));
    static_cast<void>(restore.controller().autosave_remnants());
    const app::EditorFrame frame = restore.controller().apply_frame(app::RestoreAutosave{});
    if (frame.tabs.size() != 2U || frame.document.save_state != core::SaveState::modified ||
        kept_of(restore) != before)
    {
        return "R2: the restored tab has the text, encoding, save and body line endings it was "
               "stored with, and is unsaved";
    }
    const auto &again = restore.autosave().stored();
    if (again.size() != 1U || again.back().bytes != snapshot.bytes ||
        !same_form(again.back().form, snapshot.form))
    {
        return "R3: the restored tab is stored again with the same bytes and form";
    }
    return {};
}

// 1 件ぶん。未保存でなければ空の答え（数えない）。
[[nodiscard]] std::pair<bool, std::string_view> round_trip_violation(Random &random)
{
    Editing editing;
    edited(random, editing);
    const bool unsaved = editing.controller().frame().document.save_state != core::SaveState::saved;
    const Kept before = kept_of(editing);
    applied(editing.controller(), app::AutosaveTick{});
    const auto &stored = editing.autosave().stored();
    if (!unsaved)
    {
        return {false, stored.empty() ? std::string_view{} : "a saved document is not stored"};
    }
    if (stored.size() != 1U)
    {
        return {true, "R1: an unsaved document is stored once, whatever its encoding holds"};
    }
    return {true, restored_violation(stored.back(), before)};
}

void verify_round_trip()
{
    Random random{round_trip_seed};
    std::size_t broken = 0;
    std::size_t unsaved = 0;
    for (std::size_t index = 0; index < round_trip_cases; ++index)
    {
        const auto [counted_case, violation] = round_trip_violation(random);
        unsaved += counted_case ? 1U : 0U;
        if (!violation.empty())
        {
            ++broken;
            std::fprintf(stderr, "round trip case %zu (seed %llu): %.*s\n", index,
                         static_cast<unsigned long long>(round_trip_seed),
                         static_cast<int>(violation.size()), violation.data());
        }
    }
    expect(unsaved > round_trip_cases / 2U, "most generated documents are unsaved and stored");
    expect(broken == 0U, "restoring gives back the text and the form the autosave stored (#1688)");
}

// 退避した 1 件の形を数える（5 つの文字コードと「改行なしの LF」）。
void reached(Editing &editing, std::array<std::size_t, 5> &encodings, std::size_t &bare_lf)
{
    if (editing.autosave().stored().empty())
    {
        return;
    }
    const app::AutosaveForm form = editing.autosave().stored().back().form;
    for (std::size_t at = 0; at < chosen_encodings.size(); ++at)
    {
        encodings.at(at) += form.encoding == chosen_encodings.at(at) ? 1U : 0U;
    }
    const bool no_newline = !editing.controller().text().text().contains('\n');
    bare_lf += no_newline && form.body == core::LineEnding::lf ? 1U : 0U;
}

// 生成器が 5 つの文字コードと「改行なしの LF」の文書に届くこと（未保存で退避されたもの）。
void verify_round_trip_reaches_forms()
{
    Random random{round_trip_seed};
    std::array<std::size_t, 5> encodings{};
    std::size_t bare_lf = 0;
    for (std::size_t index = 0; index < round_trip_cases; ++index)
    {
        Editing editing;
        edited(random, editing);
        applied(editing.controller(), app::AutosaveTick{});
        reached(editing, encodings, bare_lf);
    }
    expect(std::ranges::all_of(encodings, [](std::size_t count) { return count > 0U; }) &&
               bare_lf > 0U,
           "the round trip reaches every encoding and an LF document without a newline");
}
} // namespace

void verify_autosave_restore_property_scope()
{
    verify_round_trip_reaches_forms();
    verify_round_trip();
    verify_generator_is_deterministic();
    verify_generator_reaches_both_sides();
    verify_known_case();
    expect(broken_cases() == 0U,
           "restoring brings back exactly the remnants opening can open, byte for byte");
}
} // namespace nenenib::tests
