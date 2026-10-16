#pragma once

#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"

#include <optional>

namespace nenenib::application
{
// worker が終えた退避の仕事 1 つの結果（ADR 0137 の決定 4）。id は store か discard を頼んだ
// タブの退避番号、failure は失敗したときだけ値を持つ（成功なら値なし）。collect が終わった順に
// 返す。値だけを運ぶ公開 aggregate（CPP-003）。
struct AutosaveOutcome
{
    AutosaveId id;
    std::optional<AutosaveFailure> failure;
};
} // namespace nenenib::application
