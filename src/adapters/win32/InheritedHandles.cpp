#include "InheritedHandles.hpp"

namespace nenenib::adapters::win32
{
InheritedHandles::InheritedHandles(std::array<HANDLE, 3> handles) : handles_(handles)
{
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (size == 0)
    {
        return;
    }
    storage_.resize(size);
    const auto list =
        static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(static_cast<void *>(storage_.data()));
    if (InitializeProcThreadAttributeList(list, 1, 0, &size) == 0)
    {
        return;
    }
    list_ = list;
    if (UpdateProcThreadAttribute(list_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles_.data(),
                                  sizeof(HANDLE) * handles_.size(), nullptr, nullptr) == 0)
    {
        DeleteProcThreadAttributeList(list_);
        list_ = nullptr;
    }
}

InheritedHandles::~InheritedHandles()
{
    if (list_ != nullptr)
    {
        DeleteProcThreadAttributeList(list_);
    }
}

bool InheritedHandles::valid() const noexcept
{
    return list_ != nullptr;
}

LPPROC_THREAD_ATTRIBUTE_LIST InheritedHandles::get() const noexcept
{
    return list_;
}
} // namespace nenenib::adapters::win32
