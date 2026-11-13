#pragma once

#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "GuideContext.hpp"
#include "GuideEntry.hpp"

#include <array>

namespace nenenib::core
{
struct OperationGuide
{
    GuideContext context;
    std::array<GuideEntry, 3> entries;
};

// 非表示なら鍵文字列を構築しない。本文・ステータスは同じ3操作の表示値を読む。名前は language の
// 文（#1019）。言語の無い形は日本語の文（ui の試験の形・T1e で言語の形へ寄せる）。
[[nodiscard]] OperationGuide operation_guide(GuideContext context, EditMode mode,
                                             DisplayLanguage language);
[[nodiscard]] OperationGuide operation_guide(GuideContext context, EditMode mode);

// 操作 1 つの案内の表示値（鍵は shown_chord・名前は operation_text の表）。本文とステータスの案内と
// 起動画面の鍵の節（ADR 0172 の決定 2 の (d)）が同じ 1 本を使う。
[[nodiscard]] GuideEntry operation_guide_entry(EditorOperation operation, EditMode mode,
                                               DisplayLanguage language);
} // namespace nenenib::core
