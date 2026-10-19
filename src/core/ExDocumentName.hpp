#pragma once

#include "ExDocumentVerb.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// 保存・終了の命令の名前の表の 1 行（ADR 0066・CPP-012）。forced は名前の直後の `!` を受けたときの
// 条件で、空なら `!` を受けない（Issue #501）。Nib には読み取り専用の拒否が無いので、`:w!` `:wq!`
// `:x!` は `!` の無い形と同じ条件、`:q!` だけが別の条件（quit_force）になる。
struct ExDocumentName
{
    std::string_view name;
    std::size_t shortest;
    ExDocumentVerb verb;
    std::optional<ExDocumentVerb> forced;
};
} // namespace nenenib::core
