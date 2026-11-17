#pragma once

#include "FilePath.hpp"
#include "ProjectHit.hpp"

namespace nenenib::application
{
// 全文検索の当たり 1 つとそのファイル（ADR 0167 の決定 3・4）。file はフルパス（adapter がルートの
// 文字列の後ろに名前を足して作る）、hit は core の project_hits_in の答えのまま。公開 aggregate
// （CPP-003）。
struct ProjectSearchHit
{
    core::FilePath file;
    core::ProjectHit hit;
};
} // namespace nenenib::application
