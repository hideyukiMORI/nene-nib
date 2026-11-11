#pragma once

#include "OpenFailureReply.hpp"
#include "OpenedAs.hpp"

namespace nenenib::application
{
// 開く要求の行き先（ADR 0159 の決定 6・ADR 0179 の決定 3）。開けなかったときの告げ方と、開けた
// ときの開き方。どちらも呼び手ごとの今の規則で、読みが裏で揃ってから使う。公開 aggregate
// （CPP-003）。
struct OpenRequest
{
    OpenFailureReply reply;
    OpenedAs opened_as;
};
} // namespace nenenib::application
