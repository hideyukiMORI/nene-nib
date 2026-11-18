#pragma once

#include "AutosaveId.hpp"
#include "LspCompletionList.hpp"

#include <cstdint>

namespace nenenib::application
{
// 文脈の照合を通った補完の答え（ADR 0190 の決定 1・5）。session と document は要求の文脈の
// まま（面がまだ開いているかは段 B の面が session で比べる）、list は届いた順の候補（並べ替えと
// 接頭辞での絞り込みは面の側）。公開 aggregate（CPP-003）。
struct LanguageCompletion
{
    std::uint64_t session;
    AutosaveId document;
    core::LspCompletionList list;
};
} // namespace nenenib::application
