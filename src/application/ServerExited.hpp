#pragma once

#include <cstdint>

namespace nenenib::application
{
// 子プロセスが終わった知らせと終了コード（ADR 0160 の決定 6）。
// 子が終わる前にパイプへ書いた本文を渡した後に来る（出力の EOF は待たない・子孫は Job Object
// で片付く）。stop で終わらせたときは来ない。公開 aggregate（CPP-003）。
struct ServerExited
{
    std::int32_t code;
};
} // namespace nenenib::application
