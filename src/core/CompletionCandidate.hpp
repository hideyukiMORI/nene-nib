#pragma once

#include "CompletionSource.hpp"

#include <string>

namespace nenenib::core
{
// 補完の候補 1 つ（ADR 0169 の決定 2・5）。word は本文の接頭辞と置き換える語の UTF-8、source は
// 出どころ。どちらの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct CompletionCandidate
{
    std::string word;
    CompletionSource source = CompletionSource::buffer;
};

[[nodiscard]] inline bool operator==(const CompletionCandidate &left,
                                     const CompletionCandidate &right)
{
    return left.word == right.word && left.source == right.source;
}
} // namespace nenenib::core
