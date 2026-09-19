#include "SettingsIssue.hpp"

namespace nenenib::application
{
bool operator==(const SettingsIssue &issue, SettingsFailure failure) noexcept
{
    const auto *simple = std::get_if<SettingsFailure>(&issue);
    return simple != nullptr && *simple == failure;
}
} // namespace nenenib::application
