#pragma once

namespace nenenib::core
{
// application が本文・履歴と共通の非表示条件から決める案内の文脈（ADR 0079）。
enum class GuideContext
{
    hidden,
    untouched_untitled,
    other
};
} // namespace nenenib::core
