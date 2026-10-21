#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace nenenib::application
{
// Markdown の解析の要求（ADR 0155 の決定 5）。ticket は要求の券、text は UI スレッドで写した
// 本文の連続 1 本（worker と共有するので const の shared_ptr）。値だけを運ぶ公開 aggregate
// （CPP-003・ADR 0004）。
struct MarkdownRequest
{
    std::uint64_t ticket;
    std::shared_ptr<const std::string> text;
};
} // namespace nenenib::application
