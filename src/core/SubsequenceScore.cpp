#include "SubsequenceScore.hpp"

#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace nenenib::core
{
namespace
{
using Finder = std::optional<Offset> (*)(std::string_view, Offset, char32_t) noexcept;

[[nodiscard]] char32_t lower_code_point(char32_t value) noexcept
{
    return value >= U'A' && value <= U'Z' ? value + (U'a' - U'A') : value;
}

// from から先で wanted のコードポイントが始まる境目（ADR 0061 の決定 6）。境目ごとに進むので、
// 別の文字の継続バイトの並びには当たらない。
[[nodiscard]] std::optional<Offset> code_point_from(std::string_view command, Offset from,
                                                    char32_t wanted) noexcept
{
    for (Offset at = from; at.value < command.size(); at = next_code_point(command, at))
    {
        if (lower_code_point(code_point_at(command, at)) == wanted)
        {
            return at;
        }
    }
    return std::nullopt;
}

// at のバイトが code_point_from の答えと同じく wanted の始まりか（#912）。wanted は 1〜0x7F。
// コードポイントの道が見るのは from と、その後ろの継続バイトでない全バイトなので、ASCII の
// バイトはそのまま比べ、継続バイトは外す（from の継続バイトの復号値 0 は wanted と一致しない）。
// 0x80 以上の先頭バイトだけは、壊れた列の復号値まで同じ答えにするため同じ復号で比べる。
[[nodiscard]] bool byte_starts(std::string_view command, std::size_t at, char32_t wanted) noexcept
{
    constexpr unsigned char first_non_ascii = 0x80U;
    constexpr unsigned char continuation_mask = 0xC0U;
    const auto byte = static_cast<unsigned char>(command[at]);
    if (byte < first_non_ascii)
    {
        return lower_code_point(byte) == wanted;
    }
    return (byte & continuation_mask) != first_non_ascii &&
           lower_code_point(code_point_at(command, Offset{at})) == wanted;
}

// code_point_from と同じ位置を、ASCII のバイトは復号せずに探す（#912）。
[[nodiscard]] std::optional<Offset> byte_from(std::string_view command, Offset from,
                                              char32_t wanted) noexcept
{
    for (std::size_t at = from.value; at < command.size(); ++at)
    {
        if (byte_starts(command, at, wanted))
        {
            return Offset{at};
        }
    }
    return std::nullopt;
}

// 完結した UTF-8 の区間を借り、区間ごとの走査の始点と終点を全体のバイト位置へ戻す。
[[nodiscard]] std::optional<OffsetRange> found_in(std::span<const std::string_view> command,
                                                  Offset from, char32_t wanted, Finder find)
{
    std::size_t prefix = 0;
    for (const std::string_view part : command)
    {
        const Offset local{from.value > prefix ? from.value - prefix : 0};
        const auto found = find(part, local, wanted);
        if (found.has_value())
        {
            return OffsetRange{Offset{prefix + found.value().value},
                               Offset{prefix + next_code_point(part, found.value()).value}};
        }
        prefix += part.size();
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t>
scored(std::string_view query, std::span<const std::string_view> command, Finder find)
{
    Offset at{0};
    std::size_t score = 0;
    for (const std::string_view part : command)
    {
        score += part.size();
    }
    for (Offset letter{0}; letter.value < query.size(); letter = next_code_point(query, letter))
    {
        const char32_t wanted = lower_code_point(code_point_at(query, letter));
        if (wanted == U' ')
        {
            continue;
        }
        const auto found = found_in(command, at, wanted, find);
        if (!found.has_value())
        {
            return std::nullopt;
        }
        score += (found.value().begin.value - at.value) * (at.value == 0 ? 4 : 1);
        at = found.value().end;
    }
    return score;
}
} // namespace

bool byte_walkable_query(std::string_view query) noexcept
{
    constexpr unsigned char first_non_ascii = 0x80U;
    return std::ranges::all_of(query,
                               [](char value)
                               {
                                   const auto byte = static_cast<unsigned char>(value);
                                   return byte != 0U && byte < first_non_ascii;
                               });
}

std::optional<std::size_t> subsequence_score(std::string_view query,
                                             std::span<const std::string_view> command)
{
    return scored(query, command, byte_walkable_query(query) ? byte_from : code_point_from);
}

std::optional<std::size_t>
subsequence_score_by_code_points(std::string_view query, std::span<const std::string_view> command)
{
    return scored(query, command, code_point_from);
}

std::optional<std::size_t> subsequence_score_by_bytes(std::string_view query,
                                                      std::span<const std::string_view> command)
{
    return scored(query, command, byte_from);
}
} // namespace nenenib::core
