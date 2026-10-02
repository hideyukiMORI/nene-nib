#pragma once

#include "FolderBatch.hpp"
#include "FolderPort.hpp"
#include "FolderRequest.hpp"
#include "FolderShelf.hpp"
#include "Win32Worker.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace nenenib::adapters::win32
{
// 1 つの batch の件数と、1 つの列挙で出す件数の上限の既定値（ADR 0062 の決定 8）。
// #272 の実測で見直す。
inline constexpr std::size_t folder_batch_files = 1024;
inline constexpr std::size_t folder_file_limit = 8192;

// 同じフォルダを裏で読む（ADR 0062 の決定 5〜9）。
// 列挙はワーカーを借りて走り、読めた分は棚にたまる。
// 「届いた」の合図は bind で後から受け取る（渡されるまでは知らせない）。
// HWND も窓メッセージの番号も知らない。
class Win32FolderAdapter final : public application::FolderPort
{
  public:
    explicit Win32FolderAdapter(Win32Worker &worker, std::size_t batch_files = folder_batch_files,
                                std::size_t file_limit = folder_file_limit);
    ~Win32FolderAdapter() override;
    Win32FolderAdapter(const Win32FolderAdapter &) = delete;
    Win32FolderAdapter(Win32FolderAdapter &&) = delete;
    Win32FolderAdapter &operator=(const Win32FolderAdapter &) = delete;
    Win32FolderAdapter &operator=(Win32FolderAdapter &&) = delete;

    // 合図は列挙のスレッドから、たまりが空から 1 つ以上になったときだけ呼ばれる。
    void bind(std::function<void()> arrived);
    void list(application::FolderRequest request) override;
    [[nodiscard]] std::vector<application::FolderBatch> collect() override;

  private:
    Win32Worker &worker_;
    std::shared_ptr<FolderShelf> shelf_;
};
} // namespace nenenib::adapters::win32
