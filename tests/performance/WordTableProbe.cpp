#include "WordTableProbe.hpp"

#include "CompletionCandidates.hpp"
#include "CompletionDirection.hpp"
#include "CompletionList.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "FilePath.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "SelectEditMode.hpp"
#include "TextViewGeometry.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VisibleLines.hpp"
#include "WordTable.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests::performance
{
namespace
{
// 上限のすぐ下（4 KiB 引いた大きさ）。生成の最後に空白で埋めて、ちょうどこのバイト数にする。
constexpr std::size_t document_bytes = core::word_table_byte_limit - std::size_t{4} * 1024U;
// rebuilt の区間の外で `2000dd` が消す行の数（どちらの文書でも 64 KiB を超える・下で確かめる）。
constexpr std::string_view erased_lines = "2000";

constexpr std::array<std::string_view, 32> syllables{
    "ka",  "ri",  "to",  "mu",  "ne", "sa", "po", "li",  "zen", "dor", "qua",
    "vex", "bri", "mon", "tal", "fi", "go", "hu", "jin", "kel", "lom", "nar",
    "oph", "pre", "sul", "tri", "ub", "vo", "wy", "xen", "yal", "zo"};
constexpr std::array<std::string_view, 6> keywords{"auto", "const", "return", "if", "for", "int"};
constexpr std::array<std::string_view, 24> nouns{"設定",   "設計", "文書", "編集", "補完", "表示",
                                                 "日本語", "変換", "保存", "検索", "置換", "画面",
                                                 "候補",   "入力", "速度", "計測", "文字", "行列",
                                                 "時間",   "語彙", "設備", "設問", "説明", "操作"};
constexpr std::array<std::string_view, 8> particles{"は", "を", "に", "が",
                                                    "で", "と", "の", "から"};
constexpr std::array<std::string_view, 6> katakana{"エディタ", "テーマ",     "ファイル",
                                                   "タブ",     "キャレット", "バッファ"};
constexpr std::array<std::string_view, 6> english{"window",  "buffer", "completion",
                                                  "context", "table",  "word"};

[[nodiscard]] bool japanese(WordTableWorkload workload) noexcept
{
    switch (workload)
    {
    case WordTableWorkload::first_ascii:
    case WordTableWorkload::second_ascii:
    case WordTableWorkload::rebuilt_ascii:
        return false;
    case WordTableWorkload::first_japanese:
    case WordTableWorkload::second_japanese:
    case WordTableWorkload::rebuilt_japanese:
        return true;
    }
    std::unreachable();
}

[[nodiscard]] bool holds_before(WordTableWorkload workload) noexcept
{
    switch (workload)
    {
    case WordTableWorkload::second_ascii:
    case WordTableWorkload::second_japanese:
        return true;
    case WordTableWorkload::first_ascii:
    case WordTableWorkload::rebuilt_ascii:
    case WordTableWorkload::first_japanese:
    case WordTableWorkload::rebuilt_japanese:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] bool rebuilds(WordTableWorkload workload) noexcept
{
    return workload == WordTableWorkload::rebuilt_ascii ||
           workload == WordTableWorkload::rebuilt_japanese;
}

// 固定 seed の xorshift64*（乱数の API は使わない）。上位の 32 ビットを返す（線形合同法の下位の
// ビットは周期が短く、識別子の種類が偏った）。
[[nodiscard]] std::size_t stepped(std::uint64_t &seed) noexcept
{
    seed ^= seed >> 12U;
    seed ^= seed << 25U;
    seed ^= seed >> 27U;
    return static_cast<std::size_t>((seed * 2685821657736338717ULL) >> 32U);
}

template <std::size_t Size>
[[nodiscard]] std::string_view picked(const std::array<std::string_view, Size> &words,
                                      std::uint64_t &seed) noexcept
{
    return words.at(stepped(seed) % Size);
}

// 2〜4 音節の識別子（ときどき `_` でつなぐ）。音節の組で語の種類が多くなる。
[[nodiscard]] std::string identifier(std::uint64_t &seed)
{
    const std::size_t count = 2U + stepped(seed) % 3U;
    const bool snake = stepped(seed) % 2U == 0U;
    std::string word;
    for (std::size_t part = 0; part < count; ++part)
    {
        if (snake && part > 0U)
        {
            word += '_';
        }
        word += picked(syllables, seed);
    }
    return word;
}

[[nodiscard]] std::string ascii_line(std::uint64_t &seed)
{
    std::string line = "    ";
    line += picked(keywords, seed);
    line += ' ' + identifier(seed) + " = " + identifier(seed) + '(' + identifier(seed) + ", " +
            identifier(seed) + ");\n";
    return line;
}

// 助詞の前の 1 語: 片仮名・英語・名詞、ときどき名詞を 2 つ続ける（漢字の連なりが 1 語になる）。
[[nodiscard]] std::string japanese_token(std::uint64_t &seed)
{
    const std::size_t kind = stepped(seed) % 8U;
    if (kind == 0U)
    {
        return std::string{picked(katakana, seed)};
    }
    if (kind == 1U)
    {
        return ' ' + std::string{picked(english, seed)} + ' ';
    }
    std::string token{picked(nouns, seed)};
    if (kind == 2U)
    {
        token += picked(nouns, seed);
    }
    return token;
}

// 語と助詞を交互に 12 組並べた 1 文。
[[nodiscard]] std::string japanese_line(std::uint64_t &seed)
{
    std::string line;
    for (std::size_t token = 0; token < 12U; ++token)
    {
        line += japanese_token(seed);
        line += picked(particles, seed);
    }
    line += "。\n";
    return line;
}

// 最後の行は接頭辞だけ（`G` `A` でその後ろに置いたキャレットで補完する）。手前を空白で埋めて
// 本文をちょうど document_bytes にする。
[[nodiscard]] std::string_view prefix_of(WordTableWorkload workload) noexcept
{
    return japanese(workload) ? std::string_view{"設"} : std::string_view{"ka"};
}

[[nodiscard]] std::string document_of(WordTableWorkload workload)
{
    std::uint64_t seed = 1868U;
    const std::string_view prefix = prefix_of(workload);
    std::string document;
    document.reserve(document_bytes);
    while (true)
    {
        const std::string line = japanese(workload) ? japanese_line(seed) : ascii_line(seed);
        if (document.size() + line.size() + prefix.size() > document_bytes)
        {
            break;
        }
        document += line;
    }
    document.append(document_bytes - document.size() - prefix.size(), ' ');
    document += prefix;
    return document;
}

void pressed(application::EditorController &controller, std::string_view keys)
{
    for (const char key : keys)
    {
        static_cast<void>(controller.apply(application::VimKeyPress{
            core::VimKey{core::VimCharacter{static_cast<char32_t>(key)}}}));
    }
}

// 区間の外: 文書を開いて Vim にする。rebuilt は表を作ってから `2000dd` と `u` で 64 KiB を超える
// 編集を 2 回当てて表を捨てる（本文は元に戻る）。最後に `G` `A` で末尾の接頭辞の後ろへ。
[[nodiscard]] bool prepared(Editing &editing, WordTableWorkload workload,
                            const std::string &document)
{
    auto &controller = editing.controller();
    editing.files().hold(document);
    static_cast<void>(controller.apply(
        application::VisibleLines{20U, core::TextViewGeometry{800, 20, 96, 10.0F}}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\words.txt").value()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    bool dropped = true;
    if (rebuilds(workload))
    {
        static_cast<void>(controller.insert_completion(core::CompletionDirection::forward));
        const bool built = controller.word_table().has_value();
        pressed(controller, erased_lines);
        pressed(controller, "dd");
        const std::size_t erased = document.size() - controller.text().size_bytes();
        dropped =
            built && erased > core::word_table_edit_limit && !controller.word_table().has_value();
        pressed(controller, "u");
    }
    pressed(controller, "GA");
    if (holds_before(workload))
    {
        static_cast<void>(controller.insert_completion(core::CompletionDirection::forward));
    }
    return dropped && controller.text().size_bytes() == document.size() &&
           controller.word_table().has_value() == holds_before(workload);
}

// 候補があり（上限まで）、どれも接頭辞で始まり、接頭辞の範囲が末尾で終わるか。
[[nodiscard]] bool listed(const core::CompletionList &list, std::string_view prefix)
{
    if (list.candidates.empty() || list.candidates.size() > core::completion_candidate_limit ||
        list.prefix.end.value != document_bytes ||
        list.prefix.end.value - list.prefix.begin.value != prefix.size())
    {
        return false;
    }
    for (const core::CompletionCandidate &candidate : list.candidates)
    {
        if (!candidate.word.starts_with(prefix) || candidate.word == prefix)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::uint64_t checksum_of(const core::CompletionList &list) noexcept
{
    std::uint64_t value = 14695981039346656037ULL;
    for (const char byte : list.candidates.front().word)
    {
        value = (value ^ static_cast<unsigned char>(byte)) * 1099511628211ULL;
    }
    return value + list.candidates.size();
}
} // namespace

std::string word_table_input(WordTableWorkload workload)
{
    std::string input = "word-table-v1\n";
    input += japanese(workload) ? "japanese\n" : "ascii\n";
    input += rebuilds(workload) ? "rebuilt\n" : (holds_before(workload) ? "second\n" : "first\n");
    return input;
}

std::expected<std::uint64_t, ProbeFailure> word_table_completion(WordTableWorkload workload,
                                                                 const std::string &input,
                                                                 application::TimingPort &timing)
{
    if (input != word_table_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::string document = document_of(workload);
    Editing editing;
    auto &controller = editing.controller();
    if (!prepared(editing, workload, document))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    timing.mark(core::Milestone::probe_started);
    const core::CompletionList list =
        controller.insert_completion(core::CompletionDirection::forward);
    timing.mark(core::Milestone::probe_finished);
    // 区間の後（時間の外）: 表を持ち、同じキャレットのもう 1 回が同じ列を返すか。
    const core::CompletionList again =
        controller.insert_completion(core::CompletionDirection::forward);
    const std::optional<core::WordTable> &table = controller.word_table();
    if (!table.has_value() || !listed(list, prefix_of(workload)) ||
        list.candidates != again.candidates)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::size_t distinct =
        table.value().prefixed(std::string_view{}, std::numeric_limits<std::size_t>::max()).size();
    std::cerr << "documentBytes=" << document.size() << " distinctWords=" << distinct
              << " candidates=" << list.candidates.size()
              << " firstCandidate=" << list.candidates.front().word << '\n';
    return checksum_of(list);
}
} // namespace nenenib::tests::performance
