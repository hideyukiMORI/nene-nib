#pragma once

#include "FilePath.hpp"
#include "TextPosition.hpp"

namespace nenenib::application
{
// 全文検索の当たりを開いた後にキャレットを置く先（ADR 0167 の決定 5）。開く道は裏で揃うことがある
// ので、file の文書がアクティブになった意図の出口で at へ置く（`''` を置いてから PlaceCaret）。
// 公開 aggregate（CPP-003）。
struct ProjectHitLanding
{
    core::FilePath file;
    core::TextPosition at;
};
} // namespace nenenib::application
