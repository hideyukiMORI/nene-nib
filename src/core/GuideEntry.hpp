#pragma once

#include <string>
#include <string_view>

namespace nenenib::core
{
// 名前は操作表への借用、鍵は shown_chord から作った所有値。
struct GuideEntry
{
    std::string key;
    std::string_view body_name;
    std::string_view short_name;
};
} // namespace nenenib::core
