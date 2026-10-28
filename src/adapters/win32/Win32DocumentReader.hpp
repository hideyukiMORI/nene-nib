#pragma once

#include "CodePagePort.hpp"
#include "DocumentReadOutcome.hpp"
#include "DocumentReadPort.hpp"
#include "DocumentReadRequest.hpp"
#include "DocumentReadShelf.hpp"
#include "FilePort.hpp"
#include "SynchronousDocumentReader.hpp"
#include "Win32Worker.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace nenenib::adapters::win32
{
// 呼んだスレッドで揃える大きさの上限と、worker の塊の大きさ（ADR 0159 の決定 3）。
inline constexpr std::size_t synchronous_read_bytes = 64U * 1024U * 1024U;
inline constexpr std::size_t document_chunk_bytes = 64U * 1024U * 1024U;

// 開く道の読みの本物（ADR 0159 の決定 1〜3・5・6・9・ADR 0141 の追補 10）。大きさが
// synchronous_bytes 以下か、大きさを見られない・上限を超えるファイルは、呼んだスレッドで
// SynchronousDocumentReader と同じ道で揃えて Finished を返す。超えるものは予約を UI スレッドで
// 1 回確保し、worker に塊の仕事を積んで値なしを返す。進み具合と終わりは棚にたまり、合図は bind
// で後から受け取る。HWND も窓メッセージの番号も知らない。std::bad_alloc はどちらの道でも
// 境界で受けて insufficient_memory にする（決定 6）。
class Win32DocumentReader final : public application::DocumentReadPort
{
  public:
    Win32DocumentReader(Win32Worker &worker, application::FilePort &files,
                        application::CodePagePort &code_pages,
                        std::size_t synchronous_bytes = synchronous_read_bytes,
                        std::size_t chunk_bytes = document_chunk_bytes);
    ~Win32DocumentReader() override;
    Win32DocumentReader(const Win32DocumentReader &) = delete;
    Win32DocumentReader(Win32DocumentReader &&) = delete;
    Win32DocumentReader &operator=(const Win32DocumentReader &) = delete;
    Win32DocumentReader &operator=(Win32DocumentReader &&) = delete;

    // 合図は worker のスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    [[nodiscard]] std::optional<application::DocumentReadOutcome>
    request(application::DocumentReadRequest request) override;
    void cancel(std::uint64_t ticket) override;
    [[nodiscard]] std::vector<application::DocumentReadOutcome> collect() override;

  private:
    [[nodiscard]] application::DocumentReadOutcome
    synchronous(const application::DocumentReadRequest &request);

    Win32Worker &worker_;
    application::CodePagePort &code_pages_;
    SynchronousDocumentReader synchronous_;
    std::size_t synchronous_bytes_;
    std::size_t chunk_bytes_;
    std::shared_ptr<DocumentReadShelf> shelf_;
};
} // namespace nenenib::adapters::win32
