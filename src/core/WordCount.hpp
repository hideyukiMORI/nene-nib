#pragma once

#include <cstdint>
#include <string>

namespace nenenib::core
{
// 語の表の 1 行（ADR 0169 の決定 1）。word は語の UTF-8、count は出現の数。語の表の差分の列では
// 増減（負もある）を持つ。どちらの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct WordCount
{
    std::string word;
    std::int64_t count = 0;
};

[[nodiscard]] inline bool operator==(const WordCount &left, const WordCount &right)
{
    return left.word == right.word && left.count == right.count;
}
} // namespace nenenib::core
