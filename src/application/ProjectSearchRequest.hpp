#pragma once

#include "FilePath.hpp"
#include "ProjectQuery.hpp"

#include <cstdint>

namespace nenenib::application
{
// 全文検索の要求（ADR 0167 の決定 4・ADR 0004 の不変の要求値）。root は探すフォルダ（決定 2）、
// pattern は application が core::ProjectQuery::parse で作った検索語（空でない・adapter は作り
// 直さない）、ticket は application が進める券で、届いた batch がどの要求のものかを見分ける。
// 公開 aggregate（CPP-003）。
struct ProjectSearchRequest
{
    core::FilePath root;
    core::ProjectQuery pattern;
    std::uint64_t ticket;
};
} // namespace nenenib::application
