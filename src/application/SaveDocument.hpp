#pragma once

#include "FilePath.hpp"
#include "FileWriteMode.hpp"
#include "SaveIdentity.hpp"
#include "TextEncoding.hpp"

namespace nenenib::application
{
// ファイルを保存する。文字コードは意図が必ず運び、controller は状態から勝手に変えない（決定 5）。
struct SaveDocument
{
    core::FilePath path;
    core::TextEncoding encoding;
    FileWriteMode mode = FileWriteMode::replace;
    SaveIdentity identity = SaveIdentity::update_document;
};
} // namespace nenenib::application
