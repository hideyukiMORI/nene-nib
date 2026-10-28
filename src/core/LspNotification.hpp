#pragma once

#include "JsonValue.hpp"

#include <string>

namespace nenenib::core
{
// サーバーから来た通知（`method` だけで `id` を持たない・ADR 0160 の決定 5）。params が無ければ
// JsonNull。
struct LspNotification
{
    std::string method;
    JsonValue params;
};
} // namespace nenenib::core
