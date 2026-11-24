#pragma once

#include "AutosaveId.hpp"

#include <cstdint>

namespace nenenib::application
{
// 席が `didOpen` している文書 1 つの、要求を出したときの改訂番号の写し（#2004・ADR 0194 の決定 2・
// 4）。tab はタブの退避番号、revision はその文書の EditHistory::revision()（送った版と同じ数）。
// 公開 aggregate（CPP-003）。
struct OpenedRevision
{
    AutosaveId tab;
    std::uint64_t revision;
};
} // namespace nenenib::application
