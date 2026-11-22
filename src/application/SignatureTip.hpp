#pragma once

#include "AutosaveId.hpp"
#include "LineNumber.hpp"
#include "LspSignature.hpp"

namespace nenenib::application
{
// 出ている引数の情報の札（ADR 0191 の決定 6）。hover の札と違って打鍵では消えず、文書 document の
// 行 line にキャレットがあり挿入中の間だけ出る（消える条件は controller の settle_signature_tip の
// 1 か所）。公開 aggregate（CPP-003）。
struct SignatureTip
{
    AutosaveId document;
    core::LineNumber line;
    core::LspSignature signature;
};
} // namespace nenenib::application
