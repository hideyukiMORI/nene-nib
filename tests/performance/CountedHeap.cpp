#include "CountedHeap.hpp"

#include <cstddef>

// Debug は ASan が operator new / delete を持つので、その確保の統計を読む。サニタイザの無い構成
// （Release）は exe の operator new / delete を置き換えて数える。どちらも要求した byte 数の差。
#if __has_feature(address_sanitizer)
#include <sanitizer/allocator_interface.h>
#else
#include <malloc.h>

#include <cstdlib>
#endif

namespace
{
bool &counting() noexcept
{
    static bool value = false;
    return value;
}

std::int64_t &counted() noexcept
{
    static std::int64_t value = 0;
    return value;
}
} // namespace

#if __has_feature(address_sanitizer)
namespace
{
[[nodiscard]] std::int64_t allocated_now() noexcept
{
    return static_cast<std::int64_t>(__sanitizer_get_current_allocated_bytes());
}
} // namespace

namespace nenenib::tests::performance
{
void start_heap_count() noexcept
{
    counted() = allocated_now();
    counting() = true;
}

std::int64_t counted_heap_bytes() noexcept
{
    return counting() ? allocated_now() - counted() : 0;
}

void stop_heap_count() noexcept
{
    counting() = false;
}
} // namespace nenenib::tests::performance
#else
namespace
{
// CRT が覚えている確保の大きさ（要求した byte 数）。確保と解放で同じ値を数える。
[[nodiscard]] std::int64_t held_bytes(void *block) noexcept
{
    return static_cast<std::int64_t>(_msize(block));
}
} // namespace

void *operator new(std::size_t size)
{
    void *block = std::malloc(size == 0 ? 1 : size);
    if (block == nullptr)
    {
        // 窓なしの probe は確保に失敗したら続けない（例外を投げない構成）。
        std::abort();
    }
    if (counting())
    {
        counted() += held_bytes(block);
    }
    return block;
}

void operator delete(void *block) noexcept
{
    if (block == nullptr)
    {
        return;
    }
    if (counting())
    {
        counted() -= held_bytes(block);
    }
    std::free(block);
}

void operator delete(void *block, std::size_t size) noexcept
{
    static_cast<void>(size);
    ::operator delete(block);
}

namespace nenenib::tests::performance
{
void start_heap_count() noexcept
{
    counted() = 0;
    counting() = true;
}

std::int64_t counted_heap_bytes() noexcept
{
    return counted();
}

void stop_heap_count() noexcept
{
    counting() = false;
}
} // namespace nenenib::tests::performance
#endif
