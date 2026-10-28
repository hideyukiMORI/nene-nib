#include "JsonValue.hpp"

#include <functional>
#include <optional>
#include <string_view>

namespace nenenib::core
{
bool operator==(const JsonValue &left, const JsonValue &right)
{
    return left.value == right.value;
}

std::optional<std::reference_wrapper<const JsonValue>> json_member(const JsonObject &object,
                                                                   std::string_view key)
{
    // 後勝ち（RFC 8259 は重複の意味を決めない・ADR 0160 の決定 4）。後ろから探して最初に当たる鍵。
    for (auto entry = object.rbegin(); entry != object.rend(); ++entry)
    {
        if (entry->first == key)
        {
            return std::cref(entry->second);
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
