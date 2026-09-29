#pragma once

#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
using nenenib::application::FileFailure;
using nenenib::application::FilePort;
using nenenib::core::FilePath;
using Bytes = std::expected<std::string, FileFailure>;

// 偽のファイル。OS に触れずに「読めた」「読めない」「書けない」を作り分ける（ARC-007）。
class ScriptedFiles final : public FilePort
{
  public:
    void hold(Bytes content)
    {
        content_ = std::move(content);
    }

    // この経路だけ別の中身を返す（起動引数の複数のファイル）。無い経路は hold の中身を返す。
    void hold_at(std::string path, Bytes content)
    {
        by_path_.insert_or_assign(std::move(path), std::move(content));
    }

    // 文字列が違っても同じファイルと答える組（大文字と小文字の違いなど・ADR 0056 の決定 5）。
    void treat_as_same(std::string left, std::string right)
    {
        same_.emplace_back(std::move(left), std::move(right));
    }

    // read が呼ばれた回数（同じファイルを開き直しても読み直さないことを見る）。
    [[nodiscard]] std::size_t reads() const noexcept
    {
        return reads_;
    }

    void refuse_writes(FileFailure failure)
    {
        write_failure_ = failure;
    }

    [[nodiscard]] const std::string &written() const noexcept
    {
        return written_;
    }

    [[nodiscard]] const std::string &written_path() const noexcept
    {
        return written_path_;
    }

    [[nodiscard]] const std::string &read_path() const noexcept
    {
        return read_path_;
    }

    // application が渡してきた上限。値の正本が 1 つであることをテストが見る（ARC-001）。
    [[nodiscard]] std::size_t read_limit() const noexcept
    {
        return read_limit_;
    }

    [[nodiscard]] Bytes read(const FilePath &path, std::size_t maximum_bytes) override
    {
        read_path_ = std::string(path.text());
        read_limit_ = maximum_bytes;
        ++reads_;
        const auto found = by_path_.find(read_path_);
        return found == by_path_.end() ? content_ : found->second;
    }

    [[nodiscard]] std::expected<void, FileFailure> write(const FilePath &path,
                                                         std::string_view bytes) override
    {
        if (write_failure_.has_value())
        {
            return std::unexpected(write_failure_.value());
        }
        written_path_ = std::string(path.text());
        written_ = std::string(bytes);
        return {};
    }

    [[nodiscard]] bool same_file(const FilePath &left, const FilePath &right) const override
    {
        if (left == right)
        {
            return true;
        }
        const std::pair<std::string, std::string> asked{left.text(), right.text()};
        return std::ranges::any_of(same_,
                                   [&asked](const auto &pair)
                                   {
                                       return pair == asked || (pair.first == asked.second &&
                                                                pair.second == asked.first);
                                   });
    }

  private:
    Bytes content_{std::unexpected(FileFailure::not_found)};
    std::optional<FileFailure> write_failure_;
    std::string written_;
    std::string written_path_;
    std::string read_path_;
    std::size_t read_limit_ = 0;
    std::size_t reads_ = 0;
    std::map<std::string, Bytes, std::less<>> by_path_;
    std::vector<std::pair<std::string, std::string>> same_;
};
} // namespace nenenib::tests
