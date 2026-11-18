#pragma once

#include <string>

namespace nenenib::core
{
// 成分を字句で畳んだパス（collapsed_path の答え・#1825 のレビュー R1 / R2）。text は区切りを `\` に
// そろえたパス、above_root は根より上へ出る `..` があった（その `..` は上がらずに捨てた）こと。
// 公開 aggregate（CPP-003）。
struct CollapsedPath
{
    std::string text;
    bool above_root;
};
} // namespace nenenib::core
