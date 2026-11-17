#pragma once

#include <cstdint>

namespace nenenib::application
{
// 全文検索の 1 つの要求の進み具合の段（ADR 0167 の決定 4）。閉じた集合（CPP-002）。searching の
// 後に続きが来る。complete・truncated（当たりが project_search_hit_limit に届いて探すのを止めた）・
// failed（ルートを読めなかった）は最後の batch に 1 つだけ付く。
enum class ProjectSearchPhase : std::uint8_t
{
    searching,
    complete,
    truncated,
    failed
};
} // namespace nenenib::application
