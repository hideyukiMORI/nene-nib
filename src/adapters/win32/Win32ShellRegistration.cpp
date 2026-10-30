#include "Win32ShellRegistration.hpp"

namespace nenenib::adapters::win32
{
std::expected<void, application::ShellRegistrationFailure>
Win32ShellRegistration::apply(const core::ShellRegistrationPlan &)
{
    return std::unexpected(application::ShellRegistrationFailure::io_error);
}

std::expected<void, application::ShellRegistrationFailure>
Win32ShellRegistration::remove(const core::ShellRegistrationPlan &)
{
    return std::unexpected(application::ShellRegistrationFailure::io_error);
}

application::ShellRegistrationStatus
Win32ShellRegistration::status(const core::ShellRegistrationPlan &)
{
    return application::ShellRegistrationStatus::absent;
}
} // namespace nenenib::adapters::win32
