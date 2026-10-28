#pragma once

#include <windows.h>

#include <array>
#include <cstddef>
#include <vector>

namespace nenenib::adapters::win32
{
// 子プロセスに継承させる handle を 3 つ（標準入力・出力・エラー）に限る属性の列（
// PROC_THREAD_ATTRIBUTE_HANDLE_LIST・ADR 0160 の決定 2）。ほかの継承可の handle は子に渡らない。
// 列と handle の配列は CreateProcessW が戻るまで生きている必要があるので、この値が両方を持つ。
class InheritedHandles final
{
  public:
    explicit InheritedHandles(std::array<HANDLE, 3> handles);
    ~InheritedHandles();
    InheritedHandles(const InheritedHandles &) = delete;
    InheritedHandles(InheritedHandles &&) = delete;
    InheritedHandles &operator=(const InheritedHandles &) = delete;
    InheritedHandles &operator=(InheritedHandles &&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept;

  private:
    std::array<HANDLE, 3> handles_;
    std::vector<std::byte> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};
} // namespace nenenib::adapters::win32
