#include "VimPatternCompiler.hpp"

#include "VimPatternMarkKind.hpp"
#include "VimPatternRepeat.hpp"

#include <utility>

namespace nenenib::core
{
VimPatternCompiler::VimPatternCompiler(std::span<const VimPatternAtom> atoms,
                                       std::span<const VimPatternMark> marks,
                                       VimPatternLimits limits)
    : atoms_(atoms), marks_(marks), limits_(limits)
{
    tokens_.reserve(atoms.size() + marks.size());
    partners_.resize(atoms.size() + marks.size(), 0);
}

std::expected<std::vector<VimPatternInstruction>, VimPatternFailure>
VimPatternCompiler::compiled(std::span<const VimPatternAtom> atoms,
                             std::span<const VimPatternMark> marks, VimPatternLimits limits)
{
    VimPatternCompiler compiler(atoms, marks, limits);
    compiler.tokens_built();
    compiler.alternation(0, compiler.tokens_.size());
    compiler.emitted(VimPatternInstruction{VimPatternOpcode::accept, 0, 0});
    if (compiler.overflowed_)
    {
        return std::unexpected(VimPatternFailure::too_complex);
    }
    return std::move(compiler.program_);
}

// 印は parse の順に並び、立つ原子の境は減らない（量の写しと `.` の置き換えは最後の印より後ろの
// 原子だけを触る）。境 at の印は原子 at の前に置く。
void VimPatternCompiler::tokens_built()
{
    std::vector<std::size_t> opens;
    std::size_t atom = 0;
    for (std::size_t index = 0; index < marks_.size(); ++index)
    {
        for (; atom < marks_[index].at && atom < atoms_.size(); ++atom)
        {
            tokens_.push_back(atom);
        }
        const std::size_t position = tokens_.size();
        tokens_.push_back(atoms_.size() + index);
        if (marks_[index].kind == VimPatternMarkKind::open)
        {
            opens.push_back(position);
        }
        if (marks_[index].kind == VimPatternMarkKind::close && !opens.empty())
        {
            partners_.at(opens.back()) = position;
            opens.pop_back();
        }
    }
    for (; atom < atoms_.size(); ++atom)
    {
        tokens_.push_back(atom);
    }
}

bool VimPatternCompiler::is_mark(std::size_t token) const noexcept
{
    return token >= atoms_.size();
}

const VimPatternMark &VimPatternCompiler::mark_of(std::size_t token) const
{
    return marks_[token - atoms_.size()];
}

std::size_t VimPatternCompiler::emitted(VimPatternInstruction instruction)
{
    overflowed_ = overflowed_ || program_.size() >= limits_.instructions;
    if (overflowed_)
    {
        return 0;
    }
    program_.push_back(instruction);
    return program_.size() - 1;
}

// split の先の道と後の道（最短なら入れ替える）。溢れた後は命令が無いので触らない。
void VimPatternCompiler::split_aimed(std::size_t split, std::size_t body, std::size_t after,
                                     bool lazy)
{
    if (overflowed_)
    {
        return;
    }
    program_.at(split).first = lazy ? after : body;
    program_.at(split).second = lazy ? body : after;
}

void VimPatternCompiler::alternation(std::size_t begin, std::size_t end)
{
    std::vector<std::size_t> jumps;
    std::size_t piece = begin;
    for (std::size_t token = begin; token < end && !overflowed_; ++token)
    {
        const std::size_t value = tokens_.at(token);
        if (!is_mark(value))
        {
            continue;
        }
        if (mark_of(value).kind == VimPatternMarkKind::open)
        {
            token = partners_.at(token);
            continue;
        }
        // 選択: 先の枝を先の道にした split と、枝の終わりから選択の後ろへの跳び。
        const std::size_t split = emitted(VimPatternInstruction{VimPatternOpcode::split, 0, 0});
        sequence(piece, token);
        jumps.push_back(emitted(VimPatternInstruction{VimPatternOpcode::jump, 0, 0}));
        split_aimed(split, split + 1, program_.size(), false);
        piece = token + 1;
    }
    sequence(piece, end);
    for (const std::size_t jump : jumps)
    {
        if (!overflowed_)
        {
            program_.at(jump).first = program_.size();
        }
    }
}

void VimPatternCompiler::sequence(std::size_t begin, std::size_t end)
{
    for (std::size_t token = begin; token < end && !overflowed_; ++token)
    {
        const std::size_t value = tokens_.at(token);
        if (!is_mark(value))
        {
            atom_emitted(atoms_[value], value);
            continue;
        }
        // 選択の印はここに来ない（alternation が枝に分けた）。閉じは開きが飛ばす。
        group_emitted(token, partners_.at(token));
        token = partners_.at(token);
    }
}

void VimPatternCompiler::atom_emitted(const VimPatternAtom &atom, std::size_t index)
{
    const VimPatternInstruction step{VimPatternOpcode::atom, index, 0};
    switch (atom.repeat)
    {
    case VimPatternRepeat::once:
        emitted(step);
        return;
    case VimPatternRepeat::star:
    case VimPatternRepeat::lazy_star:
    {
        const std::size_t split = emitted(VimPatternInstruction{VimPatternOpcode::split, 0, 0});
        emitted(step);
        emitted(VimPatternInstruction{VimPatternOpcode::jump, split, 0});
        split_aimed(split, split + 1, program_.size(), atom.repeat == VimPatternRepeat::lazy_star);
        return;
    }
    case VimPatternRepeat::optional:
    case VimPatternRepeat::lazy_optional:
    {
        const std::size_t split = emitted(VimPatternInstruction{VimPatternOpcode::split, 0, 0});
        emitted(step);
        split_aimed(split, split + 1, program_.size(),
                    atom.repeat == VimPatternRepeat::lazy_optional);
        return;
    }
    }
    std::unreachable();
}

void VimPatternCompiler::body_emitted(std::size_t open, std::size_t close)
{
    const std::size_t capture = mark_of(tokens_.at(open)).capture.value_or(vim_pattern_no_capture);
    emitted(VimPatternInstruction{VimPatternOpcode::open, capture, 0});
    alternation(open + 1, close);
    emitted(VimPatternInstruction{VimPatternOpcode::close, capture, 0});
}

void VimPatternCompiler::optional_body_emitted(std::size_t open, std::size_t close, bool loops,
                                               bool lazy)
{
    const std::size_t split = emitted(VimPatternInstruction{VimPatternOpcode::split, 0, 0});
    body_emitted(open, close);
    if (loops)
    {
        emitted(VimPatternInstruction{VimPatternOpcode::jump, split, 0});
    }
    split_aimed(split, split + 1, program_.size(), lazy);
}

void VimPatternCompiler::group_emitted(std::size_t open, std::size_t close)
{
    if (++depth_ > limits_.nesting)
    {
        overflowed_ = true;
        return;
    }
    const auto quantity = mark_of(tokens_.at(close)).quantity;
    if (!quantity.has_value())
    {
        body_emitted(open, close);
        --depth_;
        return;
    }
    const VimPatternQuantity limits = quantity.value();
    for (std::size_t copy = 0; copy < limits.minimum && !overflowed_; ++copy)
    {
        body_emitted(open, close);
    }
    if (!limits.maximum.has_value())
    {
        optional_body_emitted(open, close, true, limits.lazy);
    }
    const std::size_t rest =
        limits.maximum.has_value() ? limits.maximum.value() - limits.minimum : 0;
    for (std::size_t copy = 0; copy < rest && !overflowed_; ++copy)
    {
        optional_body_emitted(open, close, false, limits.lazy);
    }
    --depth_;
}
} // namespace nenenib::core
