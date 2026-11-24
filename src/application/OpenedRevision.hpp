#pragma once

#include "AutosaveId.hpp"

#include <cstdint>

namespace nenenib::application
{
// 読み込み済みのタブ 1 つの、要求を出したときの写し（#2004・ADR 0194 の
// 決定 2・4・#2011 のレビュー M1・R6 の N1）。tab はタブの退避番号、
// revision はその文書の EditHistory::revision()、sent は頼んだ席がその
// 文書を `didOpen` していたか（していれば revision は送った版と同じ数）、
// saved は頼んだときに未保存の変更が無かったか（save_state_of の 1 か所）。
// 公開 aggregate（CPP-003）。
struct OpenedRevision
{
    AutosaveId tab;
    std::uint64_t revision;
    bool sent;
    bool saved;
};
} // namespace nenenib::application
