#pragma once

#include "FrameDocumentWorkload.hpp"
#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string frame_document_input(FrameDocumentWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
framed_documents(FrameDocumentWorkload workload, const std::string &input,
                 application::TimingPort &timing);
} // namespace nenenib::tests::performance
