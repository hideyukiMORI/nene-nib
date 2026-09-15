#pragma once

#include "Milestone.hpp"
#include "TimingPort.hpp"

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 節目に時刻を与える唯一の場所（ARC-007 / ADR 0011 の決定 2）。
// bind を呼ぶまでは何も積まない（--measure が無い実行がこれ）。積むのは QueryPerformanceCounter の
// 値だけで、プロセス生成時刻との対応はデストラクタが JSON を書くときに 1 度だけ計算する。
class Win32TimingAdapter final : public application::TimingPort
{
  public:
    Win32TimingAdapter() = default;
    ~Win32TimingAdapter() override;
    Win32TimingAdapter(const Win32TimingAdapter &) = delete;
    Win32TimingAdapter(Win32TimingAdapter &&) = delete;
    Win32TimingAdapter &operator=(const Win32TimingAdapter &) = delete;
    Win32TimingAdapter &operator=(Win32TimingAdapter &&) = delete;

    // --measure <out.json> が在るときだけ合成ルートが呼ぶ。ここから記録が始まる。
    void bind(std::wstring output_path);
    void mark(core::Milestone milestone) noexcept override;

  private:
    struct Mark
    {
        core::Milestone milestone;
        std::int64_t counter;
    };

    [[nodiscard]] std::int64_t microseconds_since_origin(std::int64_t counter) const noexcept;
    [[nodiscard]] std::int64_t file_time_ticks_since_origin(std::int64_t counter) const noexcept;
    [[nodiscard]] std::int64_t first_frame_ticks() const noexcept;
    [[nodiscard]] std::string report() const;
    void flush() const;

    std::wstring output_path_;
    std::vector<Mark> marks_;
    std::int64_t frequency_ = 0;
    std::int64_t origin_counter_ = 0;
    std::int64_t origin_file_time_ = 0;
    std::int64_t creation_file_time_ = 0;
    bool recording_ = false;
};
} // namespace nenenib::adapters::win32
