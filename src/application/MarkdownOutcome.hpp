#pragma once

#include "MarkdownDocument.hpp"
#include "MarkdownFailure.hpp"

#include <cstdint>
#include <expected>

namespace nenenib::application
{
// Markdown の解析の結果（ADR 0155 の決定 5・6）。ticket は要求の券で、controller は今の券の
// 結果だけを使う。値だけを運ぶ公開 aggregate（CPP-003・ADR 0004）。
struct MarkdownOutcome
{
    std::uint64_t ticket;
    std::expected<core::MarkdownDocument, core::MarkdownFailure> result;
};
} // namespace nenenib::application
