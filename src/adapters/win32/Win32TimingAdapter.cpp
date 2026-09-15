#include "Win32TimingAdapter.hpp"

#include "FileHandle.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
// 節目の置き場は bind で 1 度だけ確保する。mark は noexcept なので、そこから先は詰め替えない
// （容量を越えた節目は捨てる。200 打鍵でも 400 に満たない）。
constexpr std::size_t mark_capacity = 4096;
constexpr std::int64_t microseconds_per_second = 1'000'000;
constexpr std::int64_t file_time_ticks_per_second = 10'000'000;
constexpr std::int64_t file_time_ticks_per_millisecond = 10'000;
constexpr std::size_t fraction_digits = 4;

[[nodiscard]] std::int64_t ticks_of(const FILETIME &time) noexcept
{
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return static_cast<std::int64_t>(value.QuadPart);
}

// 100 ns の桁を ms の十進表記へ。浮動小数を使わないので、丸めの揺れがそもそも起きない。
[[nodiscard]] std::string milliseconds_text(std::int64_t file_time_ticks)
{
    const std::int64_t ticks = file_time_ticks > 0 ? file_time_ticks : 0;
    const std::string fraction = std::to_string(ticks % file_time_ticks_per_millisecond);
    return std::to_string(ticks / file_time_ticks_per_millisecond) + "." +
           std::string(fraction_digits - fraction.size(), '0') + fraction;
}
} // namespace

Win32TimingAdapter::~Win32TimingAdapter()
{
    flush();
}

void Win32TimingAdapter::bind(std::wstring output_path)
{
    LARGE_INTEGER frequency{};
    if (QueryPerformanceFrequency(&frequency) == 0 || frequency.QuadPart <= 0)
    {
        return;
    }
    FILETIME created{};
    FILETIME exited{};
    FILETIME in_kernel{};
    FILETIME in_user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &in_kernel, &in_user) == 0)
    {
        return;
    }
    // 壁時計と性能計数器の対を、間に何も挟まずに 1 組だけ控える（ADR 0011 の決定 2）。
    FILETIME now{};
    LARGE_INTEGER counter{};
    GetSystemTimePreciseAsFileTime(&now);
    if (QueryPerformanceCounter(&counter) == 0)
    {
        return;
    }
    frequency_ = frequency.QuadPart;
    origin_counter_ = counter.QuadPart;
    origin_file_time_ = ticks_of(now);
    creation_file_time_ = ticks_of(created);
    marks_.reserve(mark_capacity);
    output_path_ = std::move(output_path);
    recording_ = true;
}

void Win32TimingAdapter::mark(core::Milestone milestone) noexcept
{
    if (!recording_ || marks_.size() >= marks_.capacity())
    {
        return;
    }
    LARGE_INTEGER counter{};
    if (QueryPerformanceCounter(&counter) == 0)
    {
        return;
    }
    // 確保済みの容量の中だけに積むので、ここで割り当ては起きない（noexcept を守る）。
    marks_.push_back(Mark{milestone, counter.QuadPart});
}

std::int64_t Win32TimingAdapter::microseconds_since_origin(std::int64_t counter) const noexcept
{
    const std::int64_t delta = counter - origin_counter_;
    // 商と剰余に分けて掛ける。秒に直してから掛けないと 64 ビットを越える。
    return (delta / frequency_) * microseconds_per_second +
           (delta % frequency_) * microseconds_per_second / frequency_;
}

std::int64_t Win32TimingAdapter::file_time_ticks_since_origin(std::int64_t counter) const noexcept
{
    const std::int64_t delta = counter - origin_counter_;
    return (delta / frequency_) * file_time_ticks_per_second +
           (delta % frequency_) * file_time_ticks_per_second / frequency_;
}

std::int64_t Win32TimingAdapter::first_frame_ticks() const noexcept
{
    for (const Mark entry : marks_)
    {
        if (entry.milestone == core::Milestone::frame_presented)
        {
            return origin_file_time_ + file_time_ticks_since_origin(entry.counter) -
                   creation_file_time_;
        }
    }
    return 0;
}

std::string Win32TimingAdapter::report() const
{
    std::string text =
        "{\"processCreationToFirstFrameMs\": " + milliseconds_text(first_frame_ticks()) +
        ", \"qpcFrequency\": " + std::to_string(frequency_) + ", \"marks\": [";
    const char *separator = "";
    for (const Mark entry : marks_)
    {
        text += separator;
        text += "{\"milestone\": \"";
        text += core::milestone_name(entry.milestone);
        text += "\", \"qpcMicroseconds\": ";
        text += std::to_string(microseconds_since_origin(entry.counter));
        text += "}";
        separator = ", ";
    }
    text += "]}\n";
    return text;
}

void Win32TimingAdapter::flush() const
{
    if (!recording_ || output_path_.empty())
    {
        return;
    }
    const std::string text = report();
    const FileHandle file(CreateFileW(output_path_.c_str(), GENERIC_WRITE, 0, nullptr,
                                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid())
    {
        return;
    }
    DWORD written = 0;
    static_cast<void>(
        WriteFile(file.get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr));
}
} // namespace nenenib::adapters::win32
