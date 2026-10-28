#pragma once

#include "ServerFailure.hpp"

namespace nenenib::application
{
// 起きた後に送れない・受け取れなくなった知らせ（ADR 0160 の決定 6）。公開 aggregate（CPP-003）。
struct ServerFailed
{
    ServerFailure failure;
};
} // namespace nenenib::application
