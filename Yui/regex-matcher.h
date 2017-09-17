#pragma once
#include "regex-automaton.h"
#include <string_view>
#include <vector>
#include <memory>
#include <optional>

namespace yui
{
    struct RegexMatch
    {
        std::string_view content;
        std::vector<std::string_view> capture;
    };

    using RegexMatchOpt = std::optional<RegexMatch>;
    using RegexMatchVec = std::vector<RegexMatch>;

    class RegexMatcher
    {
    public:
        using Ptr = std::unique_ptr<RegexMatcher>;

    public:
        RegexMatcher() = default;

        bool Match(std::string_view s) const;

        RegexMatchOpt Search(std::string_view s) const;
        RegexMatchVec SearchAll(std::string_view s) const;

    protected:
        // NOTE SerachInternal is an fundamental operation
        // which is implemented differently by each derived matcher
        virtual RegexMatchOpt SerachInternal(std::string_view view, bool allow_substr) const = 0;
    };

    RegexMatcher::Ptr CreateDfaMatcher(DfaAutomaton atm);
}