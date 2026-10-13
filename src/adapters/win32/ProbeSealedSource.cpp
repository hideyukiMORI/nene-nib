#include "ProbeSealedSource.hpp"
#include <utility>
namespace nenenib::adapters::win32
{
ProbeSealedSource::ProbeSealedSource(std::shared_ptr<FileHandle> file, std::size_t length,
                                     std::shared_ptr<ProbeSourceIo> io)
    : id_(ProbeSourceId::fresh()), file_(std::move(file)), length_(length), io_(std::move(io))
{
}
std::expected<ProbeRangeRead, ProbeSourceFailure>
ProbeSealedSource::request(std::size_t offset, std::size_t count) const
{
    if (!file_)
    {
        return std::unexpected(ProbeSourceFailure::unavailable);
    }
    if (offset > length_ || count > length_ - offset || count > probe_source_block_bytes)
    {
        return std::unexpected(ProbeSourceFailure::invalid_range);
    }
    ProbeRangeRead request(id_, file_, io_);
    request.offset_ = offset;
    request.count_ = count;
    return request;
}
void ProbeSealedSource::release() noexcept
{
    file_.reset();
}
} // namespace nenenib::adapters::win32
