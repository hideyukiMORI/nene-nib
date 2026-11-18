#pragma once

#include "FilePath.hpp"
#include "LandingPlace.hpp"

namespace nenenib::application
{
// 文書を開いた後にキャレットを置く先（ADR 0167 の決定 5）。全文検索の当たりと、言語サーバーの
// 定義の場所（ADR 0190 の決定 7）が同じこの 1 つを通る。開く道は裏で揃うことがあるので、file の
// 文書がアクティブになった意図の出口で at へ置く（`''` を置いてから PlaceCaret）。公開 aggregate
// （CPP-003）。
struct ProjectHitLanding
{
    core::FilePath file;
    LandingPlace at;
};
} // namespace nenenib::application
