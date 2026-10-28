#pragma once

#include <cstdint>

namespace nenenib::application
{
// 子プロセスが終わった知らせと終了コード（ADR 0160 の決定 6）。届いた本文を全部渡した後に来る。
// stop で終わらせたときは来ない。公開 aggregate（CPP-003）。
struct ServerExited
{
    std::int32_t code;
};
} // namespace nenenib::application
