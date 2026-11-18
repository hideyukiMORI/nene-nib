#pragma once

#include "AutosaveId.hpp"
#include "Offset.hpp"

#include <cstdint>

namespace nenenib::application
{
// hover と定義へを出したときの文脈（ADR 0190 の決定 1）。document は要求を出した文書のタブの
// 退避番号、revision はその文書の EditHistory::revision()、at はキャレット。答えを使うのは、
// 同じ文書がアクティブで revision とキャレットが同じときだけ。公開 aggregate（CPP-003）。
struct LspRequestContext
{
    AutosaveId document;
    std::uint64_t revision;
    core::Offset at;
};

[[nodiscard]] constexpr bool operator==(const LspRequestContext &left,
                                        const LspRequestContext &right) noexcept
{
    return left.document == right.document && left.revision == right.revision &&
           left.at == right.at;
}
} // namespace nenenib::application
