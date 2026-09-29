#pragma once

#include "DocumentState.hpp"
#include "UnloadedDocument.hpp"

#include <memory>
#include <variant>

namespace nenenib::application
{
// 脇に置いたタブ 1 本（ADR 0059 の決定 4）。読み込み済みの束か、まだ読んでいない文書かの閉じた
// 和型。どちらも置いた後は変えないので、状態の写しは参照だけを写す（ADR 0056 の決定 2）。読む所は
// std::visit で両方の形を写す（CPP-002）。
using ParkedTab =
    std::variant<std::shared_ptr<const DocumentState>, std::shared_ptr<const UnloadedDocument>>;
} // namespace nenenib::application
