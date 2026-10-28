#pragma once

#include "CodePagePort.hpp"
#include "DocumentReadOutcome.hpp"
#include "DocumentReadPort.hpp"
#include "DocumentReadRequest.hpp"
#include "FilePort.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace nenenib::adapters::win32
{
// 開く道の読みを呼んだスレッドで揃える最小の adapter（ADR 0159 の決定 3 の同期の道だけ）。
// request は常に Finished を返し、裏の仕事を持たないので cancel と collect は何もしない。
// worker で大きなファイルを読む adapter（#993 の L2b）が置き換えるまでの合成ルートの配線。
class SynchronousDocumentReader final : public application::DocumentReadPort
{
  public:
    SynchronousDocumentReader(application::FilePort &files, application::CodePagePort &code_pages);

    [[nodiscard]] std::optional<application::DocumentReadOutcome>
    request(application::DocumentReadRequest request) override;
    void cancel(std::uint64_t ticket) override;
    [[nodiscard]] std::vector<application::DocumentReadOutcome> collect() override;

  private:
    application::FilePort &files_;
    application::CodePagePort &code_pages_;
};
} // namespace nenenib::adapters::win32
