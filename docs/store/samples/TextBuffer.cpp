// A sample file for the NeNe Nib screenshots. It is not part of the product build.
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sample
{
// A string that remembers where each line starts.
class TextBuffer
{
  public:
    explicit TextBuffer(std::string text) : text_(std::move(text))
    {
        index_lines();
    }

    [[nodiscard]] std::size_t line_count() const noexcept
    {
        return starts_.size();
    }

    [[nodiscard]] std::string_view line(std::size_t number) const noexcept
    {
        if (number >= starts_.size())
        {
            return {};
        }
        const std::size_t begin = starts_[number];
        const std::size_t end =
            number + 1 < starts_.size() ? starts_[number + 1] - 1 : text_.size();
        return std::string_view(text_).substr(begin, end - begin);
    }

    void insert(std::size_t offset, std::string_view piece)
    {
        text_.insert(offset, piece);
        index_lines();
    }

  private:
    void index_lines()
    {
        starts_.assign(1, 0);
        for (std::size_t at = 0; at < text_.size(); ++at)
        {
            if (text_[at] == '\n')
            {
                starts_.push_back(at + 1);
            }
        }
    }

    std::string text_;
    std::vector<std::size_t> starts_;
};
} // namespace sample
