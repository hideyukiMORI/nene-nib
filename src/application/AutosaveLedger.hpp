#pragma once

#include "AutosaveId.hpp"

#include <cstddef>
#include <map>

namespace nenenib::application
{
// 退避の帳面（ADR 0137 の決定 2）。タブの退避番号 → 退避したときの履歴の改訂番号。持つのは
// application の controller で、AutosaveTick の道だけが読み書きする（1 打鍵の道では触らない）。
using AutosaveLedger = std::map<AutosaveId, std::size_t>;
} // namespace nenenib::application
