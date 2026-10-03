#pragma once

#include "BookmarkPort.hpp"
#include "FileBookmarks.hpp"
#include "FileBookmarksFailure.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

namespace nenenib::tests
{
using BookmarksReading =
    std::expected<nenenib::application::FileBookmarks, nenenib::application::FileBookmarksFailure>;

// 明示登録の替え玉（ADR 0063 の決定 2・3）。仕込んだ読みを返し、書かれた値は次の読みが
// 返す（本物のファイルと同じ往復）。読みと書きの回数を数え、書きの失敗を仕込める。
class ScriptedBookmarks final : public nenenib::application::BookmarkPort
{
  public:
    explicit ScriptedBookmarks(BookmarksReading reading = nenenib::application::FileBookmarks{})
        : reading_(std::move(reading))
    {
    }

    [[nodiscard]] BookmarksReading read() override
    {
        ++reads_;
        return reading_;
    }

    [[nodiscard]] std::expected<void, nenenib::application::FileBookmarksFailure>
    write(const nenenib::application::FileBookmarks &bookmarks) override
    {
        ++writes_;
        if (failure_.has_value())
        {
            return std::unexpected(failure_.value());
        }
        written_ = bookmarks;
        reading_ = bookmarks;
        return {};
    }

    // 読んだ回数。起動・開く・本文入力では登録を読まないことを見る（決定 3）。
    [[nodiscard]] std::size_t reads() const noexcept
    {
        return reads_;
    }
    [[nodiscard]] std::size_t writes() const noexcept
    {
        return writes_;
    }
    [[nodiscard]] const std::optional<nenenib::application::FileBookmarks> &written() const noexcept
    {
        return written_;
    }
    // 次からの読みが返す値を仕込む（読みも書きも数えない）。
    void serve(BookmarksReading reading)
    {
        reading_ = std::move(reading);
    }
    void fail(std::optional<nenenib::application::FileBookmarksFailure> failure)
    {
        failure_ = failure;
    }

  private:
    BookmarksReading reading_;
    std::size_t reads_ = 0;
    std::size_t writes_ = 0;
    std::optional<nenenib::application::FileBookmarks> written_;
    std::optional<nenenib::application::FileBookmarksFailure> failure_;
};
} // namespace nenenib::tests
