#pragma once

#include "FilePath.hpp"
#include "Language.hpp"
#include "PreviewAnchor.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::application
{
// 開いていないファイルの窓の要求（ADR 0189 の決定 4・段 P2）。anchor が無ければ先頭から、あれば
// その行の前 core::finder_preview_context_lines 行から（印もその行と範囲に付ける）。language は
// 構文の色の言語（パスから application が引く・ADR 0154）。券は application が進める。値だけを
// 運ぶ公開 aggregate（CPP-003）。
struct PreviewRequest
{
    core::FilePath path;
    std::optional<PreviewAnchor> anchor;
    core::Language language;
    std::uint64_t ticket;
};
} // namespace nenenib::application
