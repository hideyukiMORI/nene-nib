#pragma once

#include "FilePath.hpp"
#include "ReadingTarget.hpp"

#include <cstdint>

namespace nenenib::application
{
// 裏で読んでいる最中の要求（ADR 0159 の決定 4・5・追補 1）。開く要求は揃うまでタブも文書も
// 作らない。まだ読んでいないタブの読みはタブを帯に残す。ticket は要求の券、read_bytes /
// total_bytes は最後に届いた進み具合（届く前は 0 / 0）、target は揃ったときの行き先（開く要求の
// 告げ方か、読むタブの退避番号）。公開 aggregate（CPP-003）。
struct DocumentReading
{
    core::FilePath path;
    std::uint64_t ticket;
    std::uint64_t read_bytes;
    std::uint64_t total_bytes;
    ReadingTarget target;
};
} // namespace nenenib::application
