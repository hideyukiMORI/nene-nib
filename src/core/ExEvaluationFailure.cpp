#include "ExEvaluationFailure.hpp"

#include <string>

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

DisplayText ex_failure_message(const ExEvaluationFailure &failure, std::string_view input)
{
    const auto *const reason = std::get_if<ExFailure>(&failure);
    if (reason == nullptr || *reason != ExFailure::unsupported_argument)
    {
        return ex_failure_message(failure);
    }
    const auto first = input.find_first_not_of(' ');
    const std::string_view shown =
        first == std::string_view::npos
            ? std::string_view{}
            : input.substr(first, input.find_last_not_of(' ') - first + 1);
    const auto message = DisplayText::parse("Not supported: " + std::string(shown));
    return message ? message.value() : ex_failure_message(failure);
}
} // namespace nenenib::core
