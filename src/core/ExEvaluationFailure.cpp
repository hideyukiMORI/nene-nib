#include "ExEvaluationFailure.hpp"

namespace nenenib::core
{
namespace
{
[[nodiscard]] DisplayText message_for(ExFailure failure)
{
    return ex_failure_message(failure);
}

[[nodiscard]] DisplayText message_for(const ThemeLookupFailure &failure)
{
    return theme_failure_message(failure);
}
} // namespace

DisplayText ex_failure_message(const ExEvaluationFailure &failure)
{
    return std::visit([](const auto &value) { return message_for(value); }, failure);
}
} // namespace nenenib::core
