#pragma once

#include "Edit.hpp"
#include "TextBuffer.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// 言語サーバーへ送った文書の版 1 つ（ADR 0161 の決定 1・3）。version は `didOpen` か `didChange`
// に添えた `EditHistory::revision()`、text はその版の本文（共有の piece table の写し）、edits は
// この版の後に本文へ入った編集（次の版の `didChange` の中身と同じ順）。古い版で届いた診断は text
// で位置を写し、edits でずらしてから置く。公開 aggregate（CPP-003）。
struct SentRevision
{
    std::uint64_t version;
    core::TextBuffer text;
    std::vector<core::Edit> edits = {};
};
} // namespace nenenib::application
