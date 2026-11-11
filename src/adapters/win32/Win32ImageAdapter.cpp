#include "Win32ImageAdapter.hpp"

#include "ImageOutcome.hpp"
#include "ImageRequest.hpp"

#include <vector>

namespace nenenib::adapters::win32
{
void Win32ImageAdapter::request(application::ImageRequest request)
{
    // 段 2 が Win32Worker にファイルの読みと WIC の復号を載せる。いまは受け取って捨てる。
    static_cast<void>(request);
}

std::vector<application::ImageOutcome> Win32ImageAdapter::collect()
{
    return {};
}
} // namespace nenenib::adapters::win32
