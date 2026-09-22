#pragma once

#include "DisplayText.hpp"
#include "VimPatternFailure.hpp"
#include "VimSearchNoticeKind.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// 検索が残した報せ（ADR 0032 の決定 5）。値は単独で妥当なので公開 aggregate（CPP-003）。
// controller が command_message に写し、次の入力で消える（Ex の結果と同じ 1 本・ARC-001）。
struct VimSearchNotice
{
    VimSearchNoticeKind kind;
    // pattern_not_found の E486 に載るパターン。ほかの kind では空。
    std::string pattern;
    // unsupported_pattern で拒否した構文。ほかの kind では空。
    std::optional<VimPatternFailure> failure;
};

// 報せの文言はここ 1 か所だけが持つ（ARC-001）。長すぎるパターンは文字の境界で切る。
[[nodiscard]] DisplayText vim_search_message(const VimSearchNotice &notice);
} // namespace nenenib::core
