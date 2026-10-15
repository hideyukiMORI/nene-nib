#pragma once

#include "AutosaveId.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"

#include <optional>
#include <string>

namespace nenenib::application
{
// 前回のプロセスが残した退避の 1 本（ADR 0137 の決定 6）。location は置き場の印で、中身は
// adapters が決める不透明な文字列（application は読まずに read / forget へ返すだけ）。
// 値だけを運ぶ公開 aggregate（CPP-003）。
struct AutosaveRemnant
{
    std::string location;
    AutosaveId id;
    std::optional<core::FilePath> path;
    core::DisplayText title;
};
} // namespace nenenib::application
