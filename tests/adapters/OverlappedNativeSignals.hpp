#pragma once
#include "ProbeReadTransport.hpp"
#include <semaphore>
namespace nenenib::tests
{
// workerを終端まで待つscope内だけで使う。transport/hookより長く生存する。
class OverlappedNativeSignals final
{
  public:
    void bind(nenenib::adapters::win32::ProbeReadTransport &transport)
    {
        using namespace nenenib::adapters::win32;
        transport.set_hook(ProbeReadHook::pending, [this] { boundary_.release(); });
        transport.set_hook(ProbeReadHook::finished,
                           [this, &transport]
                           {
                               const auto reply = transport.issue_reply();
                               if (!reply.has_value() ||
                                   reply.value().kind() != ProbeReplyKind::pending)
                               {
                                   boundary_.release();
                               }
                               finished_.release();
                           });
    }
    void wait_boundary()
    {
        boundary_.acquire();
    }
    void wait_finished()
    {
        finished_.acquire();
    }

  private:
    std::binary_semaphore boundary_{0}, finished_{0};
};
} // namespace nenenib::tests
