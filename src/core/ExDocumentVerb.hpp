#pragma once

#include <cstdint>

namespace nenenib::core
{
// 保存・終了の条件。実行は controller が既存の文書操作へ写す（ADR 0066）。
enum class ExDocumentVerb : std::uint8_t
{
    write,
    quit,
    quit_force,
    write_quit,
    update_quit,
    save_as
};
} // namespace nenenib::core
