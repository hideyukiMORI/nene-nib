#pragma once

#include "FilePath.hpp"
#include "OpenFailureReply.hpp"

#include <cstdint>

namespace nenenib::application
{
// 裏で読んでいる最中の開く要求（ADR 0159 の決定 4・5）。窓に 1 つだけで、揃うまでタブも文書も
// 作らない。ticket は今の要求の券、read_bytes / total_bytes は最後に届いた進み具合（届く前は
// 0 / 0）、reply は開けなかったときの告げ方。公開 aggregate（CPP-003）。
struct DocumentReading
{
    core::FilePath path;
    std::uint64_t ticket;
    std::uint64_t read_bytes;
    std::uint64_t total_bytes;
    OpenFailureReply reply;
};
} // namespace nenenib::application
