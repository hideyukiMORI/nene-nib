#pragma once

#include "AutosaveId.hpp"

#include <cstdint>

namespace nenenib::application
{
// 読み込み済みのタブ 1 つの、要求を出したときの改訂番号の写し（#2004・ADR 0194 の決定 2・4・#2011
// のレビュー M1）。tab はタブの退避番号、revision はその文書の EditHistory::revision()、sent
// は頼んだ 席がその文書を `didOpen` していたか（していれば revision は送った版と同じ数）。公開
// aggregate （CPP-003）。
struct OpenedRevision
{
    AutosaveId tab;
    std::uint64_t revision;
    bool sent;
};
} // namespace nenenib::application
