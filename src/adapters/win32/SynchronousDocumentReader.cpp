#include "SynchronousDocumentReader.hpp"

#include "DocumentDecoding.hpp"
#include "DocumentReadFinished.hpp"

namespace nenenib::adapters::win32
{
SynchronousDocumentReader::SynchronousDocumentReader(application::FilePort &files,
                                                     application::CodePagePort &code_pages)
    : files_(files), code_pages_(code_pages)
{
}

std::optional<application::DocumentReadOutcome>
SynchronousDocumentReader::request(application::DocumentReadRequest request)
{
    return application::DocumentReadFinished{
        request.ticket, application::read_opened_document(files_, code_pages_, request.path,
                                                          request.maximum_bytes)};
}

void SynchronousDocumentReader::cancel(std::uint64_t) {}

std::vector<application::DocumentReadOutcome> SynchronousDocumentReader::collect()
{
    return {};
}
} // namespace nenenib::adapters::win32
