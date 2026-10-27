// scope `--persisted-property` の性質の試験（#897・ADR 0020 / 0059 / 0060 / 0063 / 0114 / 0137・ADR
// 0042）。 ディスクから読む 5 つの形式（settings.v1〜v3・session.v1 /
// v2・history.v1・bookmarks.v1・自動退避の
// 台帳）の復号の関数は、バイト列を受ける純粋な口（*Codec.hpp）なので、ファイルに触れずに呼ぶ。
// 固定 seed の決定的な生成器で、正しい見本に変異（1
// バイト置換・削除・挿入・切り詰め・行の入れ替え・ 行の複製・数の巨大化・不正な
// UTF-8・NUL）を加え、次を確かめる。
//   1. 落ちない（ASan / UBSan）・止まらない。例外は捕まえないので、出ればこの実行が落ちる。
//   2. 結果は値か、その形式の復号が返す閉じた失敗（場所・書き込みの失敗は返さない）。
//   3. 復号できた値は符号化して復号し直すと同じ値になる（見本は変異の前にも確かめる）。
//   4. 復号できた値は ADR の上限と不変量を守る（数・1 始まりの位置・絶対のパス・順位の順列）。
// 自動退避のスナップショット（本文）は復号を持たずバイト列のまま返すので、この試験の対象外。
// 回数は Debug（ASan）で 1 秒以内に終わる値。50 倍の走りは rounds を手で 50 にして回す（commit
// しない）。 生成器は std::mt19937_64
// の剰余だけを使い、分布の型・時刻・環境変数は使わない（ARC-007）。
#include "AbsolutePath.hpp"
#include "AutosaveCodec.hpp"
#include "AutosaveEntry.hpp"
#include "AutosaveFailure.hpp"
#include "BookmarkCodec.hpp"
#include "EditorSettings.hpp"
#include "FileBookmarks.hpp"
#include "FileBookmarksFailure.hpp"
#include "FileHistory.hpp"
#include "FileHistoryFailure.hpp"
#include "FilePath.hpp"
#include "HistoryCodec.hpp"
#include "Session.hpp"
#include "SessionCodec.hpp"
#include "SessionFailure.hpp"
#include "SessionTab.hpp"
#include "SettingsCodec.hpp"
#include "SettingsFailure.hpp"
#include "SettingsIssue.hpp"
#include "SettingsVersion.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <format>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace application = nenenib::application;
namespace adapters = nenenib::adapters::win32;
using adapters::SettingsVersion;
using application::Session;
using application::SessionTab;
using Random = std::mt19937_64;
using Check = std::string_view (*)(std::string_view);
using Samples = std::span<const std::string_view>;
using Mutation = void (*)(Random &, std::string &);
using Lines = std::vector<std::string>;

constexpr std::size_t rounds = 1U;
constexpr std::size_t family_inputs = 600U * rounds;
constexpr std::size_t composed_inputs = 200U * rounds;
constexpr std::string_view holds{};

std::size_t &failures()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &checks()
{
    static std::size_t count = 0;
    return count;
}

void expect(bool condition, const char *message)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

// --- 見本（既存の試験の見本と同じ形）---

constexpr std::array<std::string_view, 4> settings_samples{{
    "version=3\ncolorscheme=system\nfont_family=Consolas\nfont_size=13.5\nguide=on\nwrap=on\n",
    "\xEF\xBB\xBF\r\nversion = 1\r\n colorscheme = neutral-light \r\n"
    "\vfont_family = 日本語 Font=Test\r\nfont_size=21.25\f",
    "version=2\ncolorscheme=neutral-light\nfont_family=Consolas\nfont_size=13.5\nguide=off\n",
    "version=3\ncolorscheme=neutral-light\nfont_family=Cascadia Mono\nfont_size=8\nguide=off\n"
    "wrap=off\n",
}};

constexpr std::array<std::string_view, 4> session_samples{{
    "version=2\nactive=1\n12,5,3,1,0,0,leading,C:\\work\\a b\\日本語.txt\n"
    "1,1,1,0,0,0,leading,D:/notes/x,y=z.md\n"
    "400,17,380,2,0,0,leading,\\\\server\\share\\log, 2026=09.txt\n",
    "version=2\nactive=0\n",
    "\xEF\xBB\xBFversion=1\r\nactive=0\r\n3,2,1,0,C:\\a b.txt\r\n",
    "version=2\nactive=0\n1,1,1,0,17,125.25,trailing,C:/a.txt\n",
}};

// 履歴とブックマークは同じ形（version=1 と 1 行 1 つの絶対パス）。
constexpr std::array<std::string_view, 3> path_list_samples{{
    "version=1\nC:\\work\\a b\\日本語.txt\nD:/notes/x,y=z.md\n\\\\server\\share\\log, "
    "2026=09.txt\n",
    "version=1\n",
    "\xEF\xBB\xBFversion=1\r\nC:\\a.txt",
}};

constexpr std::array<std::string_view, 3> autosave_samples{{
    "version=1\nentry=7,3,5,7.body,C:/work/b.txt\ntitle=b.txt\nentry=8,1,1,8.body,\ntitle=無題\n",
    "version=1\n",
    "\xEF\xBB\xBFversion=1\r\nentry=18446744073709551615,400,17,18446744073709551615.body,"
    "\\\\server\\share\\x, y.md\r\ntitle=x, y.md\r\n",
}};

// 挿入の断片（どの形式にも混ぜる）。区切り・鍵・数・巨大な数・パスの頭・改行の形・BOM・NUL・
// 不正な UTF-8（途切れ・サロゲート・過長・5 バイト・範囲外）・多バイト。
constexpr std::array<std::string_view, 48> fragments{{
    "\n",
    "\r\n",
    "\r",
    "\xEF\xBB\xBF",
    std::string_view("\0", 1),
    "\xC3",
    "\xED\xA0\x80",
    "\xC0\xAF",
    "\xF8\x88\x80\x80\x80",
    "\xF4\x90\x80\x80",
    "日本語",
    "\xF0\x9F\x98\x80",
    "=",
    ",",
    " ",
    "\t",
    "0",
    "1",
    "-1",
    "18446744073709551615",
    "18446744073709551616",
    "99999999999999999999999999999999",
    "1e38",
    "1e39",
    "nan",
    "inf",
    "C:\\",
    "D:/",
    "\\\\server\\share\\",
    "relative.txt",
    "version=",
    "version=1\n",
    "version=2\n",
    "version=3\n",
    "active=",
    "leading",
    "trailing",
    "entry=",
    "title=",
    ".body",
    "colorscheme=",
    "font_family=",
    "font_size=",
    "guide=",
    "wrap=",
    "on",
    "off",
    "system",
}};

// 数の巨大化で置き換える数。
constexpr std::array<std::string_view, 6> huge_numbers{{
    "18446744073709551615",
    "18446744073709551616",
    "4294967296",
    "99999999999999999999999999999999",
    "340282346638528859811704183484516925441",
    "0",
}};

// 不正な UTF-8 と NUL（変異の 1 種類として名指しで混ぜる）。
constexpr std::array<std::string_view, 6> broken_utf8{{
    std::string_view("\0", 1),
    "\xC3",
    "\xED\xA0\x80",
    "\xC0\xAF",
    "\xFF",
    "\xF4\x90\x80\x80",
}};

// --- 生成器 ---

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] char raw_byte(Random &random)
{
    return static_cast<char>(static_cast<unsigned char>(below(random, 256U)));
}

template <std::size_t Count>
[[nodiscard]] std::string_view pick(Random &random, const std::array<std::string_view, Count> &from)
{
    return from.at(below(random, Count));
}

[[nodiscard]] Lines lines_of(const std::string &text)
{
    Lines lines;
    std::size_t start = 0;
    while (start <= text.size())
    {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1U;
    }
    return lines;
}

[[nodiscard]] std::string joined(const Lines &lines)
{
    std::string text;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        text += index == 0U ? "" : "\n";
        text += lines.at(index);
    }
    return text;
}

void replace_byte(Random &random, std::string &text)
{
    text.at(below(random, text.size())) = raw_byte(random);
}

void erase_bytes(Random &random, std::string &text)
{
    text.erase(below(random, text.size()), 1U + below(random, 16U));
}

void insert_fragment(Random &random, std::string &text)
{
    text.insert(below(random, text.size() + 1U), pick(random, fragments));
}

void truncate_bytes(Random &random, std::string &text)
{
    text.resize(below(random, text.size()));
}

void swap_lines(Random &random, std::string &text)
{
    Lines lines = lines_of(text);
    std::swap(lines.at(below(random, lines.size())), lines.at(below(random, lines.size())));
    text = joined(lines);
}

void duplicate_line(Random &random, std::string &text)
{
    Lines lines = lines_of(text);
    const std::size_t at = below(random, lines.size());
    const std::string copy = lines.at(at);
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(at), copy);
    text = joined(lines);
}

[[nodiscard]] bool is_digit(char character) noexcept
{
    return character >= '0' && character <= '9';
}

// 選んだ位置から後ろの最初の数の並びを巨大な数に置き換える。数が無ければ末尾に足す。
void enlarge_number(Random &random, std::string &text)
{
    const auto from = text.begin() + static_cast<std::ptrdiff_t>(below(random, text.size()));
    const auto first = std::find_if(from, text.end(), is_digit);
    const auto last = std::find_if_not(first, text.end(), is_digit);
    text.replace(first, last, pick(random, huge_numbers));
}

void insert_broken_utf8(Random &random, std::string &text)
{
    text.insert(below(random, text.size() + 1U), pick(random, broken_utf8));
}

constexpr std::array<Mutation, 9> mutations{{replace_byte, erase_bytes, insert_fragment,
                                             truncate_bytes, swap_lines, duplicate_line,
                                             enlarge_number, insert_broken_utf8, insert_fragment}};

[[nodiscard]] std::string mutated(Random &random, Samples samples)
{
    std::string text(samples[below(random, samples.size())]);
    const std::size_t edits = 1U + below(random, 6U);
    for (std::size_t edit = 0; edit < edits && !text.empty(); ++edit)
    {
        mutations.at(below(random, mutations.size()))(random, text);
    }
    return text;
}

// 見本の行と断片を混ぜて並べる（見本から遠い形）。
[[nodiscard]] std::string composed(Random &random, Samples samples)
{
    const Lines lines = lines_of(std::string(samples[below(random, samples.size())]));
    const std::size_t pieces = 1U + below(random, 24U);
    std::string text;
    for (std::size_t piece = 0; piece < pieces; ++piece)
    {
        const bool line = below(random, 2U) == 0U;
        text += line ? std::string_view(lines.at(below(random, lines.size())))
                     : pick(random, fragments);
        text += below(random, 3U) == 0U ? "\n" : "";
    }
    return text;
}

// --- 破れの報告（縮めて 16 進で出す）---

void print_bytes(std::string_view input)
{
    const std::size_t shown = std::min<std::size_t>(input.size(), 160U);
    for (std::size_t index = 0; index < shown; ++index)
    {
        std::fprintf(stderr, " %02x",
                     static_cast<unsigned>(static_cast<unsigned char>(input[index])));
    }
    std::fprintf(stderr, "\n");
}

[[nodiscard]] bool shrunk(std::string &input, std::size_t chunk, Check check,
                          std::string_view reason)
{
    for (std::size_t at = 0; at + chunk <= input.size(); at += chunk)
    {
        std::string candidate = input;
        candidate.erase(at, chunk);
        if (check(candidate) == reason)
        {
            input = std::move(candidate);
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string minimized(std::string input, Check check)
{
    const std::string_view reason = check(input);
    std::size_t chunk = std::max<std::size_t>(input.size() / 2U, 1U);
    while (chunk > 0U)
    {
        chunk = shrunk(input, chunk, check, reason) ? std::min(chunk, input.size()) : chunk / 2U;
    }
    return input;
}

void report(const char *family, const std::string &input, Check check)
{
    const std::string_view reason = check(input);
    const std::string smallest = minimized(input, check);
    std::fprintf(stderr, "  %s: %.*s (%zu bytes, minimized to %zu):", family,
                 static_cast<int>(reason.size()), reason.data(), input.size(), smallest.size());
    print_bytes(smallest);
}

using Make = std::string (*)(Random &, Samples);

[[nodiscard]] std::size_t broken_inputs(const char *description, std::uint64_t seed, Make make,
                                        std::pair<Samples, Check> format)
{
    Random random{seed};
    const std::size_t count = make == composed ? composed_inputs : family_inputs;
    std::size_t broken = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::string input = make(random, format.first);
        if (!format.second(input).empty() && broken++ == 0U)
        {
            report(description, input, format.second);
        }
    }
    return broken;
}

void verify_family(const char *description, std::uint64_t seed, Make make,
                   std::pair<Samples, Check> format)
{
    expect(broken_inputs(description, seed, make, format) == 0U, description);
}

// --- 共通の不変量 ---

// 復号できたパスは空でなく、検証済みの UTF-8 で、制御文字を持たず、絶対（ドライブか UNC）。
[[nodiscard]] bool path_holds(const core::FilePath &path)
{
    const std::string_view text = path.text();
    return !text.empty() && core::validate_utf8(text).has_value() &&
           !core::has_control_character(text) && adapters::rooted_path_text(text);
}

[[nodiscard]] bool position_holds(core::TextPosition position) noexcept
{
    return position.line.value >= 1U && position.column.value >= 1U;
}

// --- settings.v1〜v3（ADR 0020 / 0114）---

[[nodiscard]] bool settings_failure_expected(application::SettingsFailure failure) noexcept
{
    using application::SettingsFailure;
    switch (failure)
    {
    case SettingsFailure::malformed:
    case SettingsFailure::unsupported_version:
    case SettingsFailure::unknown_theme:
    case SettingsFailure::invalid_font_family:
    case SettingsFailure::invalid_font_size:
        return true;
    case SettingsFailure::location_unavailable:
    case SettingsFailure::unreadable:
    case SettingsFailure::too_large:
    case SettingsFailure::unwritable:
    case SettingsFailure::changed_externally:
    case SettingsFailure::not_loaded:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] bool settings_issue_expected(const application::SettingsIssue &issue) noexcept
{
    const auto *const failure = std::get_if<application::SettingsFailure>(&issue);
    return failure == nullptr || settings_failure_expected(*failure);
}

[[nodiscard]] bool settings_hold(const core::EditorSettings &settings)
{
    const float points = settings.font_size.points();
    const std::string_view family = settings.font_family.text();
    return std::isfinite(points) && points >= core::FontSize::minimum_points &&
           points <= core::FontSize::maximum_points && !family.empty() &&
           !core::has_control_character(family) && family.find('\n') == std::string_view::npos;
}

[[nodiscard]] std::string_view settings_version_break(std::string_view bytes,
                                                      SettingsVersion version)
{
    const auto decoded = adapters::decode_settings(bytes, version);
    if (!decoded)
    {
        return settings_issue_expected(decoded.error()) ? holds
                                                        : "settings fail with a non-decode failure";
    }
    if (!settings_hold(decoded.value()))
    {
        return "decoded settings break their invariants";
    }
    const auto again =
        adapters::decode_settings(adapters::encode_settings(decoded.value()), SettingsVersion::v3);
    if (!again || !core::same_settings(again.value(), decoded.value()))
    {
        return "decoded settings do not round trip through v3";
    }
    return holds;
}

std::string_view settings_break(std::string_view bytes)
{
    for (const SettingsVersion version :
         {SettingsVersion::v1, SettingsVersion::v2, SettingsVersion::v3})
    {
        const std::string_view reason = settings_version_break(bytes, version);
        if (!reason.empty())
        {
            return reason;
        }
    }
    return holds;
}

// --- session.v1 / v2（ADR 0059 / 0114）---

[[nodiscard]] bool session_failure_expected(application::SessionFailure failure) noexcept
{
    using application::SessionFailure;
    switch (failure)
    {
    case SessionFailure::too_large:
    case SessionFailure::malformed:
    case SessionFailure::unsupported_version:
        return true;
    case SessionFailure::location_unavailable:
    case SessionFailure::unreadable:
    case SessionFailure::unwritable:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] bool same_tab(const SessionTab &left, const SessionTab &right)
{
    return left.path == right.path && left.caret == right.caret &&
           left.first_visible == right.first_visible && left.recency == right.recency &&
           left.first_row == right.first_row && left.horizontal == right.horizontal &&
           left.affinity == right.affinity;
}

[[nodiscard]] bool same_session(const Session &left, const Session &right)
{
    return left.active == right.active &&
           std::ranges::equal(left.tabs, right.tabs,
                              [](const SessionTab &one, const SessionTab &two)
                              { return same_tab(one, two); });
}

[[nodiscard]] bool tab_holds(const SessionTab &tab)
{
    return path_holds(tab.path) && position_holds(tab.caret) && tab.first_visible.value >= 1U &&
           std::isfinite(tab.horizontal) && tab.horizontal >= 0.0F;
}

// 順位はタブの数と同じ数の順列で、見ているタブは帯の中（タブが無ければ 0）。
[[nodiscard]] bool session_holds(const Session &session)
{
    const auto &tabs = session.tabs;
    std::vector<bool> seen(tabs.size(), false);
    const bool ranked = std::ranges::all_of(tabs,
                                            [&seen](const SessionTab &tab)
                                            {
                                                const bool fresh = tab.recency < seen.size() &&
                                                                   !seen.at(tab.recency);
                                                if (fresh)
                                                {
                                                    seen.at(tab.recency) = true;
                                                }
                                                return fresh;
                                            });
    const bool within = session.active < tabs.size() || (tabs.empty() && session.active == 0U);
    return tabs.size() <= adapters::maximum_session_tabs && ranked && within &&
           std::ranges::all_of(tabs, tab_holds);
}

std::string_view session_break(std::string_view bytes)
{
    const auto decoded = adapters::decode_session(bytes);
    if (!decoded)
    {
        return session_failure_expected(decoded.error())
                   ? holds
                   : "a session fails with a non-decode failure";
    }
    if (!session_holds(decoded.value()))
    {
        return "a decoded session breaks its invariants";
    }
    const auto encoded = adapters::encode_session(decoded.value());
    const auto again = adapters::decode_session(encoded.value_or(std::string{}));
    if (!encoded || !again || !same_session(again.value(), decoded.value()))
    {
        return "a decoded session does not round trip";
    }
    return holds;
}

// --- history.v1（ADR 0060）と bookmarks.v1（ADR 0063）---

template <typename Failure> [[nodiscard]] bool path_list_failure_expected(Failure failure) noexcept
{
    switch (failure)
    {
    case Failure::too_large:
    case Failure::malformed:
    case Failure::unsupported_version:
        return true;
    case Failure::location_unavailable:
    case Failure::unreadable:
    case Failure::unwritable:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] bool paths_hold(const std::vector<core::FilePath> &files, std::size_t limit)
{
    return files.size() <= limit && std::ranges::all_of(files, path_holds);
}

std::string_view history_break(std::string_view bytes)
{
    const auto decoded = adapters::decode_history(bytes);
    if (!decoded)
    {
        return path_list_failure_expected(decoded.error())
                   ? holds
                   : "a history fails with a non-decode failure";
    }
    if (!paths_hold(decoded.value().files, adapters::maximum_history_lines))
    {
        return "a decoded history breaks its invariants";
    }
    const auto encoded = adapters::encode_history(decoded.value());
    const auto again = adapters::decode_history(encoded.value_or(std::string{}));
    if (!encoded || !again || again.value().files != decoded.value().files)
    {
        return "a decoded history does not round trip";
    }
    return holds;
}

std::string_view bookmarks_break(std::string_view bytes)
{
    const auto decoded = adapters::decode_bookmarks(bytes);
    if (!decoded)
    {
        return path_list_failure_expected(decoded.error())
                   ? holds
                   : "bookmarks fail with a non-decode failure";
    }
    if (!paths_hold(decoded.value().files, adapters::maximum_bookmark_lines))
    {
        return "decoded bookmarks break their invariants";
    }
    const auto encoded = adapters::encode_bookmarks(decoded.value());
    const auto again = adapters::decode_bookmarks(encoded.value_or(std::string{}));
    if (!encoded || !again || again.value().files != decoded.value().files)
    {
        return "decoded bookmarks do not round trip";
    }
    return holds;
}

// --- 自動退避の台帳（ADR 0137）---

[[nodiscard]] bool entry_holds(const adapters::AutosaveEntry &entry)
{
    const bool path = !entry.path.has_value() || path_holds(entry.path.value());
    const std::string_view title = entry.title.text();
    return path && position_holds(entry.caret) && !title.empty() &&
           !core::has_control_character(title);
}

[[nodiscard]] bool same_entry(const adapters::AutosaveEntry &left,
                              const adapters::AutosaveEntry &right)
{
    return left.id == right.id && left.path == right.path &&
           left.title.text() == right.title.text() && left.caret == right.caret;
}

std::string_view autosave_break(std::string_view bytes)
{
    // 台帳の失敗は unreadable の 1 つだけ（場所と書き込みの失敗は復号から返らない）。
    const auto decoded = adapters::decode_autosave_entries(bytes);
    if (!decoded)
    {
        return decoded.error() == application::AutosaveFailure::unreadable
                   ? holds
                   : "an autosave list fails with a non-decode failure";
    }
    const auto &entries = decoded.value();
    if (entries.size() > adapters::maximum_autosave_entries ||
        !std::ranges::all_of(entries, entry_holds))
    {
        return "a decoded autosave list breaks its invariants";
    }
    const auto encoded = adapters::encode_autosave_entries(entries);
    const auto again = adapters::decode_autosave_entries(encoded.value_or(std::string{}));
    if (!encoded || !again || !std::ranges::equal(again.value(), entries, same_entry))
    {
        return "a decoded autosave list does not round trip";
    }
    return holds;
}

// --- 見本・境界・決定性 ---

// 見本は変異の前に復号できて往復する（命題 3）。
void verify_samples(Samples samples, Check check, bool (*decodes)(std::string_view),
                    const char *description)
{
    const bool all = std::ranges::all_of(samples, [check, decodes](std::string_view sample)
                                         { return decodes(sample) && check(sample).empty(); });
    expect(all, description);
}

bool settings_decodes(std::string_view bytes)
{
    for (const SettingsVersion version :
         {SettingsVersion::v1, SettingsVersion::v2, SettingsVersion::v3})
    {
        if (adapters::decode_settings(bytes, version).has_value())
        {
            return true;
        }
    }
    return false;
}

bool session_decodes(std::string_view bytes)
{
    return adapters::decode_session(bytes).has_value();
}

bool history_decodes(std::string_view bytes)
{
    return adapters::decode_history(bytes).has_value();
}

bool bookmarks_decode(std::string_view bytes)
{
    return adapters::decode_bookmarks(bytes).has_value();
}

bool autosave_decodes(std::string_view bytes)
{
    return adapters::decode_autosave_entries(bytes).has_value();
}

void verify_all_samples()
{
    verify_samples(settings_samples, settings_break, settings_decodes,
                   "the settings samples decode and round trip");
    verify_samples(session_samples, session_break, session_decodes,
                   "the session samples decode and round trip");
    verify_samples(path_list_samples, history_break, history_decodes,
                   "the history samples decode and round trip");
    verify_samples(path_list_samples, bookmarks_break, bookmarks_decode,
                   "the bookmark samples decode and round trip");
    verify_samples(autosave_samples, autosave_break, autosave_decodes,
                   "the autosave samples decode and round trip");
}

[[nodiscard]] std::string paths_of(std::size_t count)
{
    std::string text = "version=1\n";
    for (std::size_t index = 0; index < count; ++index)
    {
        text += "C:/a.txt\n";
    }
    return text;
}

[[nodiscard]] std::string session_of(std::size_t count)
{
    std::string text = "version=2\nactive=0\n";
    for (std::size_t index = 0; index < count; ++index)
    {
        text += std::format("1,1,1,{},0,0,leading,C:/{}.txt\n", index, index);
    }
    return text;
}

[[nodiscard]] std::string autosave_of(std::size_t count)
{
    std::string text = "version=1\n";
    for (std::size_t index = 0; index < count; ++index)
    {
        text += std::format("entry={},1,1,{}.body,\ntitle=t\n", index, index);
    }
    return text;
}

template <typename Value, typename Failure>
[[nodiscard]] bool refused(const std::expected<Value, Failure> &result, Failure failure)
{
    return !result.has_value() && result.error() == failure;
}

// 上限ちょうどは値、1 つ超えは閉じた失敗（ADR 0059 / 0060 / 0063 / 0137 の数）。
void verify_limits()
{
    const auto at = adapters::maximum_session_tabs;
    expect(adapters::decode_session(session_of(at)).has_value() &&
               refused(adapters::decode_session(session_of(at + 1U)),
                       application::SessionFailure::too_large),
           "a session holds 256 tabs and refuses 257");
    const auto history = adapters::maximum_history_lines;
    expect(adapters::decode_history(paths_of(history)).has_value() &&
               refused(adapters::decode_history(paths_of(history + 1U)),
                       application::FileHistoryFailure::too_large),
           "a history holds 1000 lines and refuses 1001");
    const auto marks = adapters::maximum_bookmark_lines;
    expect(adapters::decode_bookmarks(paths_of(marks)).has_value() &&
               refused(adapters::decode_bookmarks(paths_of(marks + 1U)),
                       application::FileBookmarksFailure::too_large),
           "bookmarks hold 1024 lines and refuse 1025");
    const auto entries = adapters::maximum_autosave_entries;
    expect(adapters::decode_autosave_entries(autosave_of(entries)).has_value() &&
               refused(adapters::decode_autosave_entries(autosave_of(entries + 1U)),
                       application::AutosaveFailure::unreadable),
           "an autosave list holds 256 entries and refuses 257");
}

// 頭と尾の間をパスの文字で詰め、全体を total バイトにする（長いパスの 1 行）。
[[nodiscard]] std::string padded(std::string_view head, std::string_view tail, std::size_t total)
{
    std::string text{head};
    text.append(total - head.size() - tail.size(), 'a');
    text += tail;
    return text;
}

// 復号の前段のバイト数の上限（各 codec の maximum_*_bytes）。ちょうどは値、1 バイト超えは閉じた
// 失敗。前回のタブと履歴の復号はバイト数を見ない（読む側の files.read が上限を持つ）ので対象外。
void verify_byte_limits()
{
    constexpr std::string_view mark_head = "version=1\nC:/";
    const auto marks = adapters::maximum_bookmark_bytes;
    expect(adapters::decode_bookmarks(padded(mark_head, "\n", marks)).has_value() &&
               refused(adapters::decode_bookmarks(padded(mark_head, "\n", marks + 1U)),
                       application::FileBookmarksFailure::too_large),
           "bookmarks hold 1 MiB and refuse one byte more");
    constexpr std::string_view entry_head = "version=1\nentry=0,1,1,0.body,C:/";
    constexpr std::string_view entry_tail = "\ntitle=t\n";
    const auto list = adapters::maximum_autosave_list_bytes;
    expect(adapters::decode_autosave_entries(padded(entry_head, entry_tail, list)).has_value() &&
               refused(adapters::decode_autosave_entries(padded(entry_head, entry_tail, list + 1U)),
                       application::AutosaveFailure::unreadable),
           "an autosave list holds 1 MiB and refuses one byte more");
}

void verify_generators_are_deterministic()
{
    Random left{8971U};
    Random right{8971U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        same = same && mutated(left, session_samples) == mutated(right, session_samples) &&
               composed(left, settings_samples) == composed(right, settings_samples);
    }
    expect(same, "the generators give the same inputs for the same seed");
}

void verify_mutated_families()
{
    verify_family("settings hold on mutated samples", 8901U, mutated,
                  {settings_samples, settings_break});
    verify_family("sessions hold on mutated samples", 8902U, mutated,
                  {session_samples, session_break});
    verify_family("histories hold on mutated samples", 8903U, mutated,
                  {path_list_samples, history_break});
    verify_family("bookmarks hold on mutated samples", 8904U, mutated,
                  {path_list_samples, bookmarks_break});
    verify_family("autosave lists hold on mutated samples", 8905U, mutated,
                  {autosave_samples, autosave_break});
}

void verify_composed_families()
{
    verify_family("settings hold on composed lines", 8911U, composed,
                  {settings_samples, settings_break});
    verify_family("sessions hold on composed lines", 8912U, composed,
                  {session_samples, session_break});
    verify_family("histories hold on composed lines", 8913U, composed,
                  {path_list_samples, history_break});
    verify_family("bookmarks hold on composed lines", 8914U, composed,
                  {path_list_samples, bookmarks_break});
    verify_family("autosave lists hold on composed lines", 8915U, composed,
                  {autosave_samples, autosave_break});
}
} // namespace

int main(int argc, char **argv)
{
    const std::string_view scope = argc == 2 ? argv[1] : "";
    if (argc > 2 || (argc == 2 && scope != "--persisted-property"))
    {
        std::fprintf(stderr, "Unknown persisted format test scope\n");
        return 1;
    }
    verify_generators_are_deterministic();
    verify_all_samples();
    verify_limits();
    verify_byte_limits();
    verify_mutated_families();
    verify_composed_families();
    std::printf("Persisted format properties: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
